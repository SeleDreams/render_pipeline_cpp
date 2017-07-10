#include "render_pipeline/rpcore/mount_manager.hpp"

#include <dcast.h>
#include <filename.h>
#include <virtualFileSystem.h>
#include <virtualFileMountRamdisk.h>
#include <virtualFileMountSystem.h>
#include <config_util.h>

#include <boost/filesystem.hpp>
#include <boost/dll/runtime_symbol_info.hpp>

#include <spdlog/fmt/ostr.h>

#include "render_pipeline/rppanda/stdpy/file.hpp"

namespace rpcore {

struct MountManager::Impl
{
    Impl(MountManager& self, RenderPipeline& pipeline);

    void set_write_path(const std::string& pth);

    bool get_lock(void);

    void mount(void);

    std::string find_basepath(void) const;

    void wrtie_lock(void);

    /**
     * @param[in] fname	Panda3D path (unix-style).
     */
    bool try_remove(const std::string& fname);

    /** Gets called when the manager is destructed. */
    void on_exit_cleanup(void);

public:
    MountManager& self_;
    RenderPipeline& pipeline_;

    /** This is Panda3D path (unix-style). */
    ///@{
    std::string base_path_;
    std::string lock_file_;
    std::string write_path_;
    std::string config_dir_;
    ///@}

    bool mounted_ = false;
    bool do_cleanup_ = true;
};

MountManager::Impl::Impl(MountManager& self, RenderPipeline& pipeline): self_(self), pipeline_(pipeline)
{
    lock_file_ = "instance.pid";
}

void MountManager::Impl::set_write_path(const std::string& pth)
{
    if (pth.empty())
    {
        write_path_ = "";
        lock_file_ = "instance.pid";
    }
    else
    {
        write_path_ = Filename::from_os_specific(pth).get_fullpath();
        lock_file_ = rppanda::join(pth, "instance.pid");
    }
}

bool MountManager::Impl::get_lock(void)
{
    // Check if there is a lockfile at all
    if (rppanda::isfile(lock_file_))
    {
        // Read process id from lockfile
        // TODO: implement this.
        return true;
    }
    else
    {
        wrtie_lock();
        return true;
    }
}

void MountManager::Impl::mount(void)
{
    self_.debug("Setting up virtual filesystem");
    mounted_ = true;

    auto convert_path = [](const std::string& pth) {
        return Filename::from_os_specific(pth).get_fullpath();
    };
    VirtualFileSystem* vfs = VirtualFileSystem::get_global_ptr();

    // Mount config dir as $$rpconf
    if (config_dir_.empty())
    {
        const std::string& config_dir = convert_path(rppanda::join(base_path_, "config/"));
        self_. debug("Mounting auto-detected config dir: " + config_dir);
        vfs->mount(config_dir, "/$$rpconfig", 0);
    }
    else
    {
        self_.debug("Mounting custom config dir: " + config_dir_);
        vfs->mount(convert_path(config_dir_), "/$$rpconfig", 0);
    }

    // Mount directory structure
    vfs->mount(convert_path(base_path_), "/$$rp", 0);
    vfs->mount(convert_path(rppanda::join(base_path_, "rpcore/shader")), "/$$rp/shader", 0);
    vfs->mount(convert_path(rppanda::join(base_path_, "effects")), "effects", 0);

    // Mount the pipeline temp path:
    // If no write path is specified, use a virtual ramdisk
    if (write_path_.empty())
    {
        self_.debug("Mounting ramdisk as /$$rptemp");
        vfs->mount(new VirtualFileMountRamdisk, "/$$rptemp", 0);
    }
    else
    {
        // In case an actual write path is specified:
        // Ensure the pipeline write path exists, and if not, create it
        if (!rppanda::isdir(write_path_))
        {
            self_.debug("Creating temporary path, since it does not exist yet");
            try
            {
                vfs->make_directory_full(Filename(write_path_));
            }
            catch (const std::exception& err)
            {
                self_.fatal(std::string("Failed to create temporary path: ") + err.what());
            }
        }

        self_.debug("Mounting " + write_path_ + " as /$$rptemp");
        vfs->mount(convert_path(write_path_), "/$$rptemp", 0);
    }

    auto& model_path = get_model_path();
    model_path.prepend_directory("/$$rp");
    model_path.prepend_directory("/$$rp/shader");
    model_path.prepend_directory("/$$rptemp");
}

std::string MountManager::Impl::find_basepath(void) const
{
    Filename pth = Filename::from_os_specific(rppanda::join(
        Filename::from_os_specific(boost::dll::program_location().string()), ".."));
    pth.make_absolute();

    return pth.get_fullpath();
}

void MountManager::Impl::wrtie_lock(void)
{
    // TODO: implmeent this.
}

bool MountManager::Impl::try_remove(const std::string& fname)
{
    try
    {
        self_.debug("Try to remove '" + fname + "'");
        boost::filesystem::remove(Filename(fname).to_os_specific());
        return true;
    }
    catch(...)
    {
    }
    return false;
}

void MountManager::Impl::on_exit_cleanup(void)
{
    if (do_cleanup_)
    {
        self_.debug("Cleaning up ..");

        if (!write_path_.empty())
        {
            // Try removing the lockfile
            // TODO: uncomment
            //try_remove(lock_file_);

            // Check for further tempfiles in the write path
            // We explicitely use os.listdir here instead of panda's listdir,
            // to work with actual paths.
            VirtualFileSystem* vfs = VirtualFileSystem::get_global_ptr();
            const std::string& write_path_os = Filename(write_path_).to_os_specific();
            for (const auto& fpath: boost::filesystem::directory_iterator(write_path_os))
            {
                const std::string& fname = fpath.path().filename().generic_string();
                const std::string& pth = fpath.path().generic_string();

                // Tempfiles from the pipeline start with "$$" to distinguish
                // them from user created files.
                if (rppanda::isfile(pth) && fname.substr(0, 2) == "$$")
                    try_remove(pth);
            }

            // Delete the write path if no files are left.
            if (std::count_if(
                boost::filesystem::directory_iterator(write_path_os),
                boost::filesystem::directory_iterator(),
                [](const boost::filesystem::directory_entry&){return true; }) < 1)
            {
                try
                {
                    boost::filesystem::remove(write_path_os);
                    self_.debug("Remove '" + write_path_os + "'");
                }
                catch (...)
                {
                }
            }
        }
    }
}

// ************************************************************************************************

MountManager::MountManager(RenderPipeline& pipeline): RPObject("MountManager"), impl_(std::make_unique<Impl>(*this, pipeline))
{
    set_base_path(impl_->find_basepath());

    debug("Auto-Detected base path to " + impl_->base_path_);
}

MountManager::~MountManager(void)
{
    impl_->on_exit_cleanup();
}

inline const std::string& MountManager::get_write_path(void) const
{
    return impl_->write_path_;
}

void MountManager::set_write_path(const std::string& pth)
{
    impl_->set_write_path(pth);
}

inline const std::string& MountManager::get_base_path(void) const
{
    return impl_->base_path_;
}

void MountManager::set_base_path(const std::string& pth)
{
    debug("Set base path to '" + pth + "'");
    impl_->base_path_ = Filename::from_os_specific(pth).get_fullpath();
}

inline const std::string& MountManager::get_config_dir(void) const
{
    return impl_->config_dir_;
}

void MountManager::set_config_dir(const std::string& pth)
{
    impl_->config_dir_ = Filename::from_os_specific(pth).get_fullpath();
}

inline bool MountManager::get_do_cleanup(void) const
{
    return impl_->do_cleanup_;
}

inline void MountManager::set_do_cleanup(bool cleanup)
{
    impl_->do_cleanup_ = cleanup;
}

bool MountManager::get_lock(void)
{
    return impl_->get_lock();
}

inline bool MountManager::is_mounted(void) const
{
    return impl_->mounted_;
}

void MountManager::mount(void)
{
    impl_->mount();
}

void MountManager::unmount(void)
{
    throw std::runtime_error("TODO");
}

std::string MountManager::convert_to_physical_path(const std::string& path)
{
    Filename plugin_dir_in_vfs(path);
    plugin_dir_in_vfs.standardize();

    VirtualFileSystem* vfs = VirtualFileSystem::get_global_ptr();
    for (int k=0, k_end=vfs->get_num_mounts(); k < k_end; ++k)
    {
        const std::string mount_point = vfs->get_mount(k)->get_mount_point().to_os_specific();
        const std::string plugin_dir_in_vfs_string = plugin_dir_in_vfs.to_os_specific();

        // /{mount_point}/...
        if (mount_point.substr(0, 2) == std::string("$$") && plugin_dir_in_vfs_string.find(mount_point) == 1)
        {
            boost::filesystem::path physical_plugin_dir = DCAST(VirtualFileMountSystem, vfs->get_mount(k))->get_physical_filename().to_os_specific();
            physical_plugin_dir /= plugin_dir_in_vfs_string.substr(1+mount_point.length());
            return physical_plugin_dir.string();
        }
    }
    
    RPObject::global_error("MountManager", fmt::format("Cannot convert to physical path from Panda Path ({}).", path.c_str()));

    return "";
}

}
