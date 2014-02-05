#include "main.h"
#include "debug.h"
#include <stdint.h>

namespace util
{

std::int64_t to_int64(const std::string &version_string)
{
    std::uint64_t version;
    try {
        version = std::stoll(version_string);
    } catch (std::exception& e) {
        pok_warning("Illegal version string '%s'. Setting version to 0.", version_string.c_str());
        version = 0;
    }
    return version;
}

std::int64_t to_int64(const std::shared_ptr<const std::string> version_string)
{
    return to_int64(version_string->data());
}


int grab_inode_generation_token(void)
{
    const std::string inode_base_key = "inode_generation";

    std::unique_ptr<std::string> keyVersion;
    KineticStatus status = PRIV->kinetic->GetVersion(inode_base_key, keyVersion);
    if (status.ok())
        PRIV->inum_base = util::to_int64(keyVersion->data());
    else if (status.statusCode() == kinetic::StatusCode::REMOTE_NOT_FOUND)
        PRIV->inum_base = 0;
    else
        return -EIO;

    KineticRecord empty("", std::to_string(PRIV->inum_base + UINT16_MAX), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    status = PRIV->kinetic->Put(inode_base_key, PRIV->inum_base ? std::to_string(PRIV->inum_base) : "", WriteMode::REQUIRE_SAME_VERSION, empty);
    if (status.statusCode() ==  kinetic::StatusCode::REMOTE_VERSION_MISMATCH)
        return grab_inode_generation_token();
    if (!status.ok())
        return -EIO;
    return 0;
}

ino_t generate_inode_number(void)
{
    std::lock_guard<std::mutex> locker(PRIV->lock);
    PRIV->inum_counter++;
    if (PRIV->inum_counter == UINT16_MAX){
       if( grab_inode_generation_token() )
           pok_error(" Error encountered when attempting to refresh inode generation token. "
                     " Cannot generate inode numbers. Quitting. ");
       PRIV->inum_counter = 0;
    }
    return (ino_t) (PRIV->inum_base +  PRIV->inum_counter);
}

std::string path_to_filename(const std::string &path)
{
    return path.substr(path.find_last_of("/:") + 1);
}

}
