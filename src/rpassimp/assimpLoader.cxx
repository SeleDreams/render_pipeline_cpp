/**
 * PANDA 3D SOFTWARE
 * Copyright (c) Carnegie Mellon University.  All rights reserved.
 *
 * All use of this software is subject to the terms of the revised BSD
 * license.  You should have received a copy of this license along
 * with this source code in a file named "LICENSE."
 *
 * @file assimpLoader.cxx
 * @author rdb
 * @date 2011-03-29
 */

#include "assimpLoader.h"

#include <regex>
#include <numeric>

#include "geomNode.h"
#include "luse.h"
#include "geomVertexWriter.h"
#include "geomPoints.h"
#include "geomLines.h"
#include "geomTriangles.h"
#include "pnmFileTypeRegistry.h"
#include "pnmImage.h"
#include "materialAttrib.h"
#include "textureAttrib.h"
#include "cullFaceAttrib.h"
#include "ambientLight.h"
#include "directionalLight.h"
#include "spotlight.h"
#include "pointLight.h"
#include "look_at.h"
#include "texturePool.h"
#include "character.h"
#include "animBundle.h"
#include "animBundleNode.h"
#include "animChannelMatrixXfmTable.h"
#include "pvector.h"

#include "pandaIOSystem.h"
#include "pandaLogger.h"

#include <assimp/postprocess.h>

#include <render_pipeline/rpcore/util/rpmaterial.hpp>
#include <render_pipeline/rpcore/util/rprender_state.hpp>
#include <render_pipeline/rpcore/util/primitives.hpp>

#include "config_assimp.h"

namespace rpassimp {

using std::ostringstream;
using std::stringstream;

struct BoneWeight
{
    CPT(JointVertexTransform) joint_vertex_xform;
    float weight;

    BoneWeight(CPT(JointVertexTransform) joint_vertex_xform, float weight)
        : joint_vertex_xform(joint_vertex_xform), weight(weight)
    {}
};

typedef pvector<BoneWeight> BoneWeightList;

AssimpLoader::AssimpLoader() : _error(false), _geoms(nullptr)
{
    PandaLogger::set_default();
    _importer.SetIOHandler(new PandaIOSystem);
}

AssimpLoader::~AssimpLoader()
{
    _importer.FreeScene();
}

void AssimpLoader::get_extensions(std::string &ext) const
{
    aiString aexts;
    _importer.GetExtensionList(aexts);

    std::string tmp = aexts.C_Str();

    // The format is like: *.mdc;*.mdl;*.mesh.xml;*.mot
    const std::regex token(";");
    std::vector<std::string> parsed(std::sregex_token_iterator(tmp.begin(), tmp.end(), token, -1), std::sregex_token_iterator());

    if (parsed.empty())
        return;

    ext = std::accumulate(parsed.begin()+1, parsed.end(), parsed.begin()->substr(2), [](const std::string& a, const std::string& b) {
        return a + " " + b.substr(2);
    });
}

bool AssimpLoader::read(const Filename &filename)
{
    _filename = filename;

    unsigned int flags = aiProcess_Triangulate | aiProcess_GenUVCoords |
        aiProcess_ValidateDataStructure | aiProcess_TransformUVCoords;

    if (assimp_calc_tangent_space) {
        flags |= aiProcess_CalcTangentSpace;
    }
    if (assimp_join_identical_vertices) {
        flags |= aiProcess_JoinIdenticalVertices;
    }
    if (assimp_improve_cache_locality) {
        flags |= aiProcess_ImproveCacheLocality;
    }
    if (assimp_remove_redundant_materials) {
        flags |= aiProcess_RemoveRedundantMaterials;
    }
    if (assimp_fix_infacing_normals) {
        flags |= aiProcess_FixInfacingNormals;
    }
    if (assimp_optimize_meshes) {
        flags |= aiProcess_OptimizeMeshes;
    }
    if (assimp_optimize_graph) {
        flags |= aiProcess_OptimizeGraph;
    }
    if (assimp_flip_winding_order) {
        flags |= aiProcess_FlipWindingOrder;
    }
    if (assimp_gen_normals)
    {
        if (assimp_smooth_normal_angle == 0.0)
        {
            flags |= aiProcess_GenNormals;
        }
        else
        {
            flags |= aiProcess_GenSmoothNormals;
            _importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE,
                assimp_smooth_normal_angle);
        }
    }

    _scene = _importer.ReadFile(_filename.c_str(), flags);

    if (_scene == nullptr)
    {
        _error = true;
        return false;
    }

    _error = false;
    return true;
}

void AssimpLoader::build_graph()
{
    nassertv(_scene != nullptr); // read() must be called first
    nassertv(!_error);        // and have succeeded

                                // Protect the import process
    MutexHolder holder(_lock);

    _root = new ModelRoot(_filename.get_basename());

    // Import all of the embedded textures first.
    _textures = new PT(Texture)[_scene->mNumTextures];
    for (size_t i = 0; i < _scene->mNumTextures; ++i) {
        load_texture(i);
    }

    // Then the materials.
    _mat_states = new CPT(RenderState)[_scene->mNumMaterials];
    for (size_t i = 0; i < _scene->mNumMaterials; ++i) {
        load_material(i);
    }

    // And then the meshes.
    _geoms = new PT(Geom)[_scene->mNumMeshes];
    _geom_matindices = new unsigned int[_scene->mNumMeshes];
    for (size_t i = 0; i < _scene->mNumMeshes; ++i) {
        load_mesh(i);
    }

    // And now the node structure.
    if (_scene->mRootNode != nullptr) {
        load_node(*_scene->mRootNode, _root);
    }

    // And lastly, the lights.
    for (size_t i = 0; i < _scene->mNumLights; ++i) {
        load_light(*_scene->mLights[i]);
    }

    delete[] _textures;
    delete[] _mat_states;
    delete[] _geoms;
    delete[] _geom_matindices;
}

const aiNode *AssimpLoader::find_node(const aiNode &root, const aiString &name)
{
    const aiNode* node = nullptr;

    if (root.mName == name) {
        return &root;
    }
    else {
        for (size_t i = 0; i < root.mNumChildren; ++i) {
            node = find_node(*root.mChildren[i], name);
            if (node) {
                return node;
            }
        }
    }

    return nullptr;
}

void AssimpLoader::load_texture(size_t index)
{
    const aiTexture &tex = *_scene->mTextures[index];

    PT(Texture) ptex = Texture::make_texture();

    if (tex.mHeight == 0)
    {
        // Compressed texture.
        rpassimp_cat.debug()
            << "Reading embedded compressed texture with format " << tex.achFormatHint << " and size " << tex.mWidth << std::endl;
        stringstream str;
        str.write((char*)tex.pcData, tex.mWidth);

        if (strncmp(tex.achFormatHint, "dds", 3) == 0)
        {
            ptex->read_dds(str);
        }
        else
        {
            const PNMFileTypeRegistry *reg = PNMFileTypeRegistry::get_global_ptr();
            PNMFileType *ftype;
            PNMImage img;

            // Work around a bug in Assimp, it sometimes writes jp instead of jpg
            if (strncmp(tex.achFormatHint, "jp\0", 3) == 0)
            {
                ftype = reg->get_type_from_extension("jpg");
            }
            else
            {
                ftype = reg->get_type_from_extension(tex.achFormatHint);
            }

            if (img.read(str, "", ftype))
            {
                ptex->load(img);
            }
            else
            {
                ptex = nullptr;
            }
        }
    }
    else
    {
        rpassimp_cat.debug()
            << "Reading embedded raw texture with size " << tex.mWidth << "x" << tex.mHeight << std::endl;

        ptex->setup_2d_texture(tex.mWidth, tex.mHeight, Texture::T_unsigned_byte, Texture::F_rgba);
        PTA_uchar data = ptex->modify_ram_image();

        size_t p = 0;
        for (size_t i = 0; i < tex.mWidth * tex.mHeight; ++i)
        {
            const aiTexel &texel = tex.pcData[i];
            data[p++] = texel.b;
            data[p++] = texel.g;
            data[p++] = texel.r;
            data[p++] = texel.a;
        }
    }

    // ostringstream path; path << "tmp" << index << ".png";
    // ptex->write(path.str());

    _textures[index] = ptex;
}

void AssimpLoader::load_texture_stage(const aiMaterial &mat, const aiTextureType &ttype, CPT(TextureAttrib) &tattr)
{
    aiString path;
    aiTextureMapping mapping;
    unsigned int uvindex;
    float blend;
    aiTextureOp op;
    aiTextureMapMode mapmode;

    const auto texture_count = mat.GetTextureCount(ttype);
    if (texture_count == 0)
    {
        if (ttype == aiTextureType_DIFFUSE)
        {
            PT(TextureStage) stage = new TextureStage("basecolor-0");
            stage->set_sort(0);
            tattr = DCAST(TextureAttrib, tattr->add_on_stage(stage, rpcore::load_empty_basecolor()));
        }
        else if (ttype == aiTextureType_NORMALS)
        {
            PT(TextureStage) stage = new TextureStage("normal-10");
            stage->set_sort(10);
            tattr = DCAST(TextureAttrib, tattr->add_on_stage(stage, rpcore::load_empty_normal()));
        }
    }

    for (unsigned int i = 0; i < texture_count; ++i)
    {
        mat.GetTexture(ttype, i, &path, &mapping, nullptr, &blend, &op, &mapmode);

        if (aiReturn_SUCCESS != mat.Get(AI_MATKEY_UVWSRC(ttype, i), uvindex))
        {
            // If there's no texture coordinate set for this texture, assume that
            // it's the same as the index on the stack.  TODO: if there's only one
            // set on the mesh, force everything to use just the first stage.
            uvindex = i;
        }

        stringstream str;
        str << uvindex;
        PT(TextureStage) stage = new TextureStage(str.str());
        if (uvindex > 0)
            stage->set_texcoord_name(InternalName::get_texcoord_name(str.str()));

        PT(Texture) ptex = nullptr;

        // I'm not sure if this is the right way to handle it, as I couldn't find
        // much information on embedded textures.
        if (path.data[0] == '*')
        {
            long num = strtol(path.data + 1, nullptr, 10);
            ptex = _textures[num];
        }
        else if (path.length > 0)
        {
            Filename fn = Filename::from_os_specific(std::string(path.data, path.length));

            // Try to find the file by moving up twice in the hierarchy.
            VirtualFileSystem *vfs = VirtualFileSystem::get_global_ptr();
            Filename dir(_filename);
            _filename.make_canonical();
            dir = _filename.get_dirname();

            // Quake 3 BSP doesn't specify an extension for textures.
            if (vfs->is_regular_file(Filename(dir, fn))) {
                fn = Filename(dir, fn);
            }
            else if (vfs->is_regular_file(Filename(dir, fn + ".tga"))) {
                fn = Filename(dir, fn + ".tga");
            }
            else if (vfs->is_regular_file(Filename(dir, fn + ".jpg"))) {
                fn = Filename(dir, fn + ".jpg");
            }
            else {
                dir = _filename.get_dirname();
                if (vfs->is_regular_file(Filename(dir, fn))) {
                    fn = Filename(dir, fn);
                }
                else if (vfs->is_regular_file(Filename(dir, fn + ".tga"))) {
                    fn = Filename(dir, fn + ".tga");
                }
                else if (vfs->is_regular_file(Filename(dir, fn + ".jpg"))) {
                    fn = Filename(dir, fn + ".jpg");
                }
            }

            ptex = TexturePool::load_texture(fn);
        }

        if (ptex != nullptr)
        {
            if (ttype == aiTextureType_DIFFUSE)
            {
                const auto current_format = ptex->get_format();
                if (current_format == Texture::Format::F_rgb)
                    ptex->set_format(Texture::Format::F_srgb);
                else if (current_format == Texture::Format::F_rgba)
                    ptex->set_format(Texture::Format::F_srgb_alpha);

                stage->set_sort(0);
            }
            else if (ttype == aiTextureType_NORMALS)
            {
                stage->set_sort(10);
            }

            tattr = DCAST(TextureAttrib, tattr->add_on_stage(stage, ptex));
        }
    }
}

void AssimpLoader::load_material(size_t index)
{
    const aiMaterial &mat = *_scene->mMaterials[index];

    CPT(RenderState) state = RenderState::make_empty();

    aiColor3D col;
    bool have;
    int ival;
    ai_real fval;
    aiString name;
    aiShadingMode shading = aiShadingMode_Blinn;

    // XXX a lot of this is untested.

    // First do the material attribute.
    rpcore::RPMaterial rpmat;
    have = false;

    if (aiReturn_SUCCESS == mat.Get(AI_MATKEY_NAME, name))
    {
        rpmat->set_name(name.C_Str());
        rpassimp_cat.debug() << "Processing material: " << name.C_Str() << std::endl;
    }

    if (aiReturn_SUCCESS == mat.Get(AI_MATKEY_SHADING_MODEL, shading))
    {
        if (shading != aiShadingMode_Blinn && shading != aiShadingMode_Phong)
        {
            rpassimp_cat.warning() << "Unknown shading model: " << shading << std::endl;
            shading = aiShadingMode_Blinn;
        }
    }

    if (shading == aiShadingMode_Blinn || shading == aiShadingMode_Phong)
    {
        if (mat.GetTextureCount(aiTextureType_DIFFUSE) > 0)
        {
            rpmat.set_base_color(LColor(1));
            have = true;
        }
        else if (aiReturn_SUCCESS == mat.Get(AI_MATKEY_COLOR_DIFFUSE, col))
        {
            rpmat.set_base_color(LColor(col.r, col.g, col.b, 1));
            have = true;
        }

        if (mat.GetTextureCount(aiTextureType_NORMALS) > 0)
        {
            rpmat.set_normal_factor(1.0f);
            have = true;
        }

        if ((aiReturn_SUCCESS == mat.Get(AI_MATKEY_OPACITY, fval)) && fval != 1.0f)
        {
            rpmat.set_shading_model(rpcore::RPMaterial::ShadingModel::TRANSPARENT_MODEL);
            rpmat.set_alpha(fval);
            have = true;
        }

        if (aiReturn_SUCCESS == mat.Get(AI_MATKEY_SHININESS, fval))
        {
            // shininess is 0 ~ inf
            rpmat.set_roughness(1.0f / (1 + std::log2f((std::max)(0.0f, fval) + 1)));
            have = true;
        }

        if (aiReturn_SUCCESS == mat.Get(AI_MATKEY_REFRACTI, fval))
        {
            rpmat.set_specular_ior(fval);
            have = true;
        }
    }
    else if (shading == aiShadingMode_Fresnel)
    {
        //if (aiReturn_SUCCESS == mat.Get(AI_MATKEY_COLOR_EMISSIVE, col))
        //{
        //    rpmat.set_shading_model(rpcore::RPMaterial::ShadingModel::EMISSIVE_MODEL);
        //    rpmat.set_base_color(LColor(col.r, col.g, col.b, 1));
        //    have = true;
        //}
    }

    if (have)
        state = state->add_attrib(MaterialAttrib::make(rpmat.get_material()));

    // Wireframe.
    if (aiReturn_SUCCESS == mat.Get(AI_MATKEY_ENABLE_WIREFRAME, ival))
    {
        if (ival)
            state = state->add_attrib(RenderModeAttrib::make(RenderModeAttrib::M_wireframe));
        else
            state = state->add_attrib(RenderModeAttrib::make(RenderModeAttrib::M_filled));
    }

    // Backface culling.  Not sure if this is also supposed to set the twoside
    // flag in the material, I'm guessing not.
    if (aiReturn_SUCCESS == mat.Get(AI_MATKEY_TWOSIDED, ival))
    {
        if (ival)
            state = state->add_attrib(CullFaceAttrib::make(CullFaceAttrib::M_cull_none));
        else
            state = state->add_attrib(CullFaceAttrib::make_default());
    }

    // And let's not forget the textures!
    CPT(TextureAttrib) tattr = DCAST(TextureAttrib, TextureAttrib::make());
    load_texture_stage(mat, aiTextureType_DIFFUSE, tattr);
    load_texture_stage(mat, aiTextureType_NORMALS, tattr);

    // specular and roughness
    {
        PT(TextureStage) specular_stage = new TextureStage("specular-20");
        specular_stage->set_sort(20);
        tattr = DCAST(TextureAttrib, tattr->add_on_stage(specular_stage, rpcore::load_empty_specular()));

        PT(TextureStage) roughness_stage = new TextureStage("roughness-30");
        roughness_stage->set_sort(30);
        tattr = DCAST(TextureAttrib, tattr->add_on_stage(roughness_stage, rpcore::load_empty_roughness()));
    }

    //load_texture_stage(mat, aiTextureType_LIGHTMAP, tattr);

    if (tattr->get_num_on_stages() > 0)
        state = state->add_attrib(tattr);

    _mat_states[index] = state;
}

void AssimpLoader::create_joint(Character *character, CharacterJointBundle *bundle, PartGroup *parent, const aiNode &node)
{
    const aiMatrix4x4 &t = node.mTransformation;
    LMatrix4 mat(t.a1, t.b1, t.c1, t.d1,
        t.a2, t.b2, t.c2, t.d2,
        t.a3, t.b3, t.c3, t.d3,
        t.a4, t.b4, t.c4, t.d4);
    PT(CharacterJoint) joint = new CharacterJoint(character, bundle, parent, node.mName.C_Str(), mat);

    rpassimp_cat.debug()
        << "Creating joint for: " << node.mName.C_Str() << std::endl;

    for (size_t i = 0; i < node.mNumChildren; ++i) {
        if (_bonemap.find(node.mChildren[i]->mName.C_Str()) != _bonemap.end()) {
            create_joint(character, bundle, joint, *node.mChildren[i]);
        }
    }
}

void AssimpLoader::create_anim_channel(const aiAnimation &anim, AnimBundle *bundle, AnimGroup *parent, const aiNode &node)
{
    PT(AnimChannelMatrixXfmTable) group = new AnimChannelMatrixXfmTable(parent, node.mName.C_Str());

    // See if there is a channel for this node
    aiNodeAnim *node_anim = nullptr;
    for (size_t i = 0; i < anim.mNumChannels; ++i) {
        if (anim.mChannels[i]->mNodeName == node.mName) {
            node_anim = anim.mChannels[i];
        }
    }

    if (node_anim)
    {
        rpassimp_cat.debug()
            << "Found channel for node: " << node.mName.C_Str() << std::endl;
        // rpassimp_cat.debug() << "Num Position Keys " <<
        // node_anim->mNumPositionKeys << std::endl; rpassimp_cat.debug() << "Num
        // Rotation Keys " << node_anim->mNumRotationKeys << std::endl;
        // rpassimp_cat.debug() << "Num Scaling Keys " << node_anim->mNumScalingKeys
        // << std::endl;

        // Convert positions
        PTA_stdfloat tablex = PTA_stdfloat::empty_array(node_anim->mNumPositionKeys);
        PTA_stdfloat tabley = PTA_stdfloat::empty_array(node_anim->mNumPositionKeys);
        PTA_stdfloat tablez = PTA_stdfloat::empty_array(node_anim->mNumPositionKeys);
        for (size_t i = 0; i < node_anim->mNumPositionKeys; ++i) {
            tablex[i] = node_anim->mPositionKeys[i].mValue.x;
            tabley[i] = node_anim->mPositionKeys[i].mValue.y;
            tablez[i] = node_anim->mPositionKeys[i].mValue.z;
        }
        group->set_table('x', tablex);
        group->set_table('y', tabley);
        group->set_table('z', tablez);

        // Convert rotations
        PTA_stdfloat tableh = PTA_stdfloat::empty_array(node_anim->mNumRotationKeys);
        PTA_stdfloat tablep = PTA_stdfloat::empty_array(node_anim->mNumRotationKeys);
        PTA_stdfloat tabler = PTA_stdfloat::empty_array(node_anim->mNumRotationKeys);
        for (size_t i = 0; i < node_anim->mNumRotationKeys; ++i) {
            aiQuaternion ai_quat = node_anim->mRotationKeys[i].mValue;
            LVecBase3 hpr = LQuaternion(ai_quat.w, ai_quat.x, ai_quat.y, ai_quat.z).get_hpr();
            tableh[i] = hpr.get_x();
            tablep[i] = hpr.get_y();
            tabler[i] = hpr.get_z();
        }
        group->set_table('h', tableh);
        group->set_table('p', tablep);
        group->set_table('r', tabler);

        // Convert scales
        PTA_stdfloat tablei = PTA_stdfloat::empty_array(node_anim->mNumScalingKeys);
        PTA_stdfloat tablej = PTA_stdfloat::empty_array(node_anim->mNumScalingKeys);
        PTA_stdfloat tablek = PTA_stdfloat::empty_array(node_anim->mNumScalingKeys);
        for (size_t i = 0; i < node_anim->mNumScalingKeys; ++i) {
            tablei[i] = node_anim->mScalingKeys[i].mValue.x;
            tablej[i] = node_anim->mScalingKeys[i].mValue.y;
            tablek[i] = node_anim->mScalingKeys[i].mValue.z;
        }
        group->set_table('i', tablei);
        group->set_table('j', tablej);
        group->set_table('k', tablek);
    }
    else
    {
        rpassimp_cat.debug() << "No channel found for node: " << node.mName.C_Str() << std::endl;
    }

    for (size_t i = 0; i < node.mNumChildren; ++i) {
        if (_bonemap.find(node.mChildren[i]->mName.C_Str()) != _bonemap.end()) {
            create_anim_channel(anim, bundle, group, *node.mChildren[i]);
        }
    }
}

void AssimpLoader::load_mesh(size_t index)
{
    const aiMesh &mesh = *_scene->mMeshes[index];

    // Check if we need to make a Character
    PT(Character) character = nullptr;
    if (mesh.HasBones()) {
        rpassimp_cat.debug()
            << "Creating character for " << mesh.mName.C_Str() << std::endl;

        // Find and add all bone nodes to the bone map
        for (size_t i = 0; i < mesh.mNumBones; ++i) {
            const aiBone &bone = *mesh.mBones[i];
            const aiNode *node = find_node(*_scene->mRootNode, bone.mName);
            _bonemap[bone.mName.C_Str()] = node;
        }

        // Now create a character from the bones
        character = new Character(mesh.mName.C_Str());
        PT(CharacterJointBundle) bundle = character->get_bundle(0);
        PT(PartGroup) skeleton = new PartGroup(bundle, "<skeleton>");

        for (size_t i = 0; i < mesh.mNumBones; ++i) {
            const aiBone &bone = *mesh.mBones[i];

            // Find the root bone node
            const aiNode *root = _bonemap[bone.mName.C_Str()];
            while (root->mParent && _bonemap.find(root->mParent->mName.C_Str()) != _bonemap.end()) {
                root = root->mParent;
            }

            // Don't process this root if we already have a joint for it
            if (character->find_joint(root->mName.C_Str())) {
                continue;
            }

            create_joint(character, bundle, skeleton, *root);
        }
    }

    // Create transform blend table
    PT(TransformBlendTable) tbtable = new TransformBlendTable;
    pvector<BoneWeightList> bone_weights(mesh.mNumVertices);
    if (character) {
        for (size_t i = 0; i < mesh.mNumBones; ++i) {
            const aiBone &bone = *mesh.mBones[i];
            CharacterJoint *joint = character->find_joint(bone.mName.C_Str());
            if (joint == nullptr) {
                rpassimp_cat.debug()
                    << "Could not find joint for bone: " << bone.mName.C_Str() << std::endl;
                continue;
            }

            CPT(JointVertexTransform) jvt = new JointVertexTransform(joint);

            for (size_t j = 0; j < bone.mNumWeights; ++j) {
                const aiVertexWeight &weight = bone.mWeights[j];

                bone_weights[weight.mVertexId].push_back(BoneWeight(jvt, weight.mWeight));
            }
        }
    }

    // Create the vertex format.
    PT(GeomVertexArrayFormat) aformat = new GeomVertexArrayFormat;
    aformat->add_column(InternalName::get_vertex(), 3, Geom::NT_stdfloat, Geom::C_point);
    if (mesh.HasNormals())
    {
        aformat->add_column(InternalName::get_normal(), 3, Geom::NT_stdfloat, Geom::C_normal);
    }

    if (mesh.HasVertexColors(0))
    {
        aformat->add_column(InternalName::get_color(), 4, Geom::NT_stdfloat, Geom::C_color);
    }

    const unsigned int num_uvs = mesh.GetNumUVChannels();
    if (num_uvs > 0)
    {
        // UV sets are named texcoord, texcoord.1, texcoord.2...
        aformat->add_column(InternalName::get_texcoord(), mesh.mNumUVComponents[0], Geom::NT_stdfloat, Geom::C_texcoord);
        for (unsigned int u = 1; u < num_uvs; ++u)
        {
            ostringstream out;
            out << u;
            aformat->add_column(InternalName::get_texcoord_name(out.str()), mesh.mNumUVComponents[u], Geom::NT_stdfloat, Geom::C_texcoord);
        }
    }

    PT(GeomVertexArrayFormat) tb_aformat = new GeomVertexArrayFormat;
    tb_aformat->add_column(InternalName::make("transform_blend"), 1, Geom::NT_uint16, Geom::C_index);

    // Check to see if we need to convert any animations
    for (size_t i = 0; i < _scene->mNumAnimations; ++i) {
        aiAnimation &ai_anim = *_scene->mAnimations[i];
        bool convert_anim = false;

        rpassimp_cat.debug()
            << "Checking to see if anim (" << ai_anim.mName.C_Str() << ") matches character (" << mesh.mName.C_Str() << ")\n";
        for (size_t j = 0; j < ai_anim.mNumChannels; ++j) {
            rpassimp_cat.debug()
                << "Searching for " << ai_anim.mChannels[j]->mNodeName.C_Str() << " in bone map" << std::endl;
            if (_bonemap.find(ai_anim.mChannels[j]->mNodeName.C_Str()) != _bonemap.end()) {
                convert_anim = true;
                break;
            }
        }

        if (convert_anim) {
            rpassimp_cat.debug()
                << "Found animation (" << ai_anim.mName.C_Str() << ") for character (" << mesh.mName.C_Str() << ")\n";

            // Now create the animation
            unsigned int frames = 0;
            for (size_t j = 0; j < ai_anim.mNumChannels; ++j) {
                if (ai_anim.mChannels[j]->mNumPositionKeys > frames) {
                    frames = ai_anim.mChannels[j]->mNumPositionKeys;
                }
                if (ai_anim.mChannels[j]->mNumRotationKeys > frames) {
                    frames = ai_anim.mChannels[j]->mNumRotationKeys;
                }
                if (ai_anim.mChannels[j]->mNumScalingKeys > frames) {
                    frames = ai_anim.mChannels[j]->mNumScalingKeys;
                }
            }
            PN_stdfloat fps = frames / (ai_anim.mTicksPerSecond * ai_anim.mDuration);
            rpassimp_cat.debug()
                << "FPS " << fps << std::endl;
            rpassimp_cat.debug()
                << "Frames " << frames << std::endl;

            PT(AnimBundle) bundle = new AnimBundle(mesh.mName.C_Str(), fps, frames);
            PT(AnimGroup) skeleton = new AnimGroup(bundle, "<skeleton>");

            for (size_t i = 0; i < mesh.mNumBones; ++i) {
                const aiBone &bone = *mesh.mBones[i];

                // Find the root bone node
                const aiNode *root = _bonemap[bone.mName.C_Str()];
                while (root->mParent && _bonemap.find(root->mParent->mName.C_Str()) != _bonemap.end()) {
                    root = root->mParent;
                }

                // Only convert root nodes
                if (root->mName == bone.mName) {
                    create_anim_channel(ai_anim, bundle, skeleton, *root);

                    // Attach the animation to the character node
                    PT(AnimBundleNode) bundle_node = new AnimBundleNode(bone.mName.C_Str(), bundle);
                    character->add_child(bundle_node);
                }
            }
        }
    }

    // TODO: if there is only one UV set, hackily iterate over the texture
    // stages and clear the texcoord name things

    PT(GeomVertexFormat) format = new GeomVertexFormat;
    format->add_array(aformat);
    if (character) {
        format->add_array(tb_aformat);

        GeomVertexAnimationSpec aspec;
        aspec.set_panda();
        format->set_animation(aspec);
    }

    // Create the GeomVertexData.
    std::string name(mesh.mName.data, mesh.mName.length);
    PT(GeomVertexData) vdata = new GeomVertexData(name, GeomVertexFormat::register_format(format), Geom::UH_static);
    if (character) {
        vdata->set_transform_blend_table(tbtable);
    }
    vdata->unclean_set_num_rows(mesh.mNumVertices);

    // Read out the vertices.
    GeomVertexWriter vertex(vdata, InternalName::get_vertex());
    for (size_t i = 0; i < mesh.mNumVertices; ++i) {
        const aiVector3D &vec = mesh.mVertices[i];
        vertex.add_data3(vec.x, vec.y, vec.z);
    }

    // Now the normals, if any.
    if (mesh.HasNormals()) {
        GeomVertexWriter normal(vdata, InternalName::get_normal());
        for (size_t i = 0; i < mesh.mNumVertices; ++i) {
            const aiVector3D &vec = mesh.mNormals[i];
            normal.add_data3(vec.x, vec.y, vec.z);
        }
    }

    // Vertex colors, if any.  We only import the first set.
    if (mesh.HasVertexColors(0)) {
        GeomVertexWriter color(vdata, InternalName::get_color());
        for (size_t i = 0; i < mesh.mNumVertices; ++i) {
            const aiColor4D &col = mesh.mColors[0][i];
            color.add_data4(col.r, col.g, col.b, col.a);
        }
    }

    // Now the texture coordinates.
    if (num_uvs > 0)
    {
        // UV sets are named texcoord, texcoord.1, texcoord.2...
        GeomVertexWriter texcoord0(vdata, InternalName::get_texcoord());
        switch (mesh.mNumUVComponents[0])
        {
            case 1:
            {
                for (size_t i = 0; i < mesh.mNumVertices; ++i)
                {
                    const aiVector3D &vec = mesh.mTextureCoords[0][i];
                    texcoord0.add_data1(vec.x);
                }
                break;
            }

            case 2:
            {
                for (size_t i = 0; i < mesh.mNumVertices; ++i)
                {
                    const aiVector3D &vec = mesh.mTextureCoords[0][i];
                    texcoord0.add_data2(vec.x, vec.y);
                }
                break;
            }

            case 3:
            {
                for (size_t i = 0; i < mesh.mNumVertices; ++i)
                {
                    const aiVector3D &vec = mesh.mTextureCoords[0][i];
                    texcoord0.add_data3(vec.x, vec.y, vec.z);
                }
                break;
            }

            default:
                break;
        }

        for (unsigned int u = 1; u < num_uvs; ++u)
        {
            ostringstream out;
            out << u;
            GeomVertexWriter texcoord(vdata, InternalName::get_texcoord_name(out.str()));
            switch (mesh.mNumUVComponents[0])
            {
                case 1:
                {
                    for (size_t i = 0; i < mesh.mNumVertices; ++i)
                    {
                        const aiVector3D &vec = mesh.mTextureCoords[u][i];
                        texcoord.add_data1(vec.x);
                    }
                    break;
                }

                case 2:
                {
                    for (size_t i = 0; i < mesh.mNumVertices; ++i)
                    {
                        const aiVector3D &vec = mesh.mTextureCoords[u][i];
                        texcoord.add_data2(vec.x, vec.y);
                    }
                    break;
                }

                case 3:
                {
                    for (size_t i = 0; i < mesh.mNumVertices; ++i)
                    {
                        const aiVector3D &vec = mesh.mTextureCoords[u][i];
                        texcoord.add_data3(vec.x, vec.y, vec.z);
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    // Now the transform blend table
    if (character) {
        GeomVertexWriter transform_blend(vdata, InternalName::get_transform_blend());

        for (size_t i = 0; i < mesh.mNumVertices; ++i) {
            TransformBlend tblend;

            for (size_t j = 0; j < bone_weights[i].size(); ++j) {
                tblend.add_transform(bone_weights[i][j].joint_vertex_xform, bone_weights[i][j].weight);
            }
            transform_blend.add_data1i(tbtable->add_blend(tblend));
        }

        tbtable->set_rows(SparseArray::lower_on(vdata->get_num_rows()));
    }

    // Now read out the primitives.  Keep in mind that we called ReadFile with
    // the aiProcess_Triangulate flag earlier, so we don't have to worry about
    // polygons.
    PT(GeomPoints) points = new GeomPoints(Geom::UH_static);
    PT(GeomLines) lines = new GeomLines(Geom::UH_static);
    PT(GeomTriangles) triangles = new GeomTriangles(Geom::UH_static);

    // Now add the vertex indices.
    for (size_t i = 0; i < mesh.mNumFaces; ++i) {
        const aiFace &face = mesh.mFaces[i];

        if (face.mNumIndices == 0) {
            // It happens, strangely enough.
            continue;
        }
        else if (face.mNumIndices == 1) {
            points->add_vertex(face.mIndices[0]);
            points->close_primitive();
        }
        else if (face.mNumIndices == 2) {
            lines->add_vertices(face.mIndices[0], face.mIndices[1]);
            lines->close_primitive();
        }
        else if (face.mNumIndices == 3) {
            triangles->add_vertices(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
            triangles->close_primitive();
        }
        else {
            nassertd(false) continue;
        }
    }

    // Create a geom and add the primitives to it.
    PT(Geom) geom = new Geom(vdata);
    if (points->get_num_primitives() > 0) {
        geom->add_primitive(points);
    }
    if (lines->get_num_primitives() > 0) {
        geom->add_primitive(lines);
    }
    if (triangles->get_num_primitives() > 0) {
        geom->add_primitive(triangles);
    }

    _geoms[index] = geom;
    _geom_matindices[index] = mesh.mMaterialIndex;

    if (character) {
        _charmap[mesh.mName.C_Str()] = character;
    }
}

void AssimpLoader::load_node(const aiNode &node, PandaNode *parent)
{
    PT(PandaNode) pnode;
    PT(Character) character;

    // Skip nodes we've converted to joints
    if (_bonemap.find(node.mName.C_Str()) != _bonemap.end()) {
        return;
    }

    // Create the node and give it a name.
    std::string name(node.mName.data, node.mName.length);
    if (node.mNumMeshes > 0) {
        pnode = new GeomNode(name);
    }
    else {
        pnode = new PandaNode(name);
    }

    if (_charmap.find(node.mName.C_Str()) != _charmap.end()) {
        character = _charmap[node.mName.C_Str()];
        parent->add_child(character);
    }
    else {
        parent->add_child(pnode);
    }

    // Load in the transformation matrix.
    const aiMatrix4x4 &t = node.mTransformation;
    if (!t.IsIdentity()) {
        LMatrix4 mat(t.a1, t.b1, t.c1, t.d1,
            t.a2, t.b2, t.c2, t.d2,
            t.a3, t.b3, t.c3, t.d3,
            t.a4, t.b4, t.c4, t.d4);
        pnode->set_transform(TransformState::make_mat(mat));
    }

    for (size_t i = 0; i < node.mNumChildren; ++i) {
        load_node(*node.mChildren[i], pnode);
    }

    if (node.mNumMeshes > 0) {
        // Remember, we created this as GeomNode earlier.
        PT(GeomNode) gnode = DCAST(GeomNode, pnode);
        size_t meshIndex;

        // If there's only mesh, don't bother using a per-geom state.
        if (node.mNumMeshes == 1)
        {
            meshIndex = node.mMeshes[0];
            gnode->add_geom(_geoms[meshIndex], _mat_states[_geom_matindices[meshIndex]]);
        }
        else
        {
            for (size_t i = 0; i < node.mNumMeshes; ++i)
            {
                meshIndex = node.mMeshes[i];
                gnode->add_geom(_geoms[node.mMeshes[i]], _mat_states[_geom_matindices[meshIndex]]);
            }
        }

        if (character) {
            rpassimp_cat.debug() << "Adding char to geom\n";
            character->add_child(gnode);
        }
    }
}

void AssimpLoader::load_light(const aiLight &light)
{
    std::string name(light.mName.data, light.mName.length);
    rpassimp_cat.debug() << "Found light '" << name << "'\n";

    aiColor3D col;
    aiVector3D vec;

    switch (light.mType)
    {
        case aiLightSource_DIRECTIONAL:
        {
            PT(DirectionalLight) dlight = new DirectionalLight(name);
            _root->add_child(dlight);

            col = light.mColorDiffuse;
            dlight->set_color(LColor(col.r, col.g, col.b, 1));

            col = light.mColorSpecular;
            dlight->set_specular_color(LColor(col.r, col.g, col.b, 1));

            vec = light.mPosition;
            dlight->set_point(LPoint3(vec.x, vec.y, vec.z));

            vec = light.mDirection;
            dlight->set_direction(LVector3(vec.x, vec.y, vec.z));
            break;
        }

        case aiLightSource_POINT:
        {
            PT(PointLight) plight = new PointLight(name);
            _root->add_child(plight);

            col = light.mColorDiffuse;
            plight->set_color(LColor(col.r, col.g, col.b, 1));

            col = light.mColorSpecular;
            plight->set_specular_color(LColor(col.r, col.g, col.b, 1));

            vec = light.mPosition;
            plight->set_point(LPoint3(vec.x, vec.y, vec.z));

            plight->set_attenuation(LVecBase3(light.mAttenuationConstant,
                light.mAttenuationLinear,
                light.mAttenuationQuadratic));
            break;
        }

        case aiLightSource_SPOT:
        {
            PT(Spotlight) plight = new Spotlight(name);
            _root->add_child(plight);

            col = light.mColorDiffuse;
            plight->set_color(LColor(col.r, col.g, col.b, 1));

            col = light.mColorSpecular;
            plight->set_specular_color(LColor(col.r, col.g, col.b, 1));

            plight->set_attenuation(LVecBase3(light.mAttenuationConstant,
                light.mAttenuationLinear,
                light.mAttenuationQuadratic));

            plight->get_lens()->set_fov(light.mAngleOuterCone);
            // TODO: translate mAngleInnerCone to an exponent, somehow

            // This *should* be about right.
            vec = light.mDirection;
            LPoint3 pos(light.mPosition.x, light.mPosition.y, light.mPosition.z);
            LQuaternion quat;
            ::look_at(quat, LPoint3(vec.x, vec.y, vec.z), LVector3::up());
            plight->set_transform(TransformState::make_pos_quat_scale(pos, quat, LVecBase3(1, 1, 1)));
            break;
        }

        // This is a somewhat recent addition to Assimp, so let's be kind to those
        // that don't have an up-to-date version of Assimp.
        case 0x4: //aiLightSource_AMBIENT:
            // This is handled below.
            break;

        default:
            rpassimp_cat.warning() << "Light '" << name << "' has an unknown type!\n";
            return;
    }

    // If there's an ambient color, add it as ambient light.
    col = light.mColorAmbient;
    LVecBase4 ambient(col.r, col.g, col.b, 0);
    if (ambient != LVecBase4::zero()) {
        PT(AmbientLight) alight = new AmbientLight(name);
        alight->set_color(ambient);
        _root->add_child(alight);
    }
}

}
