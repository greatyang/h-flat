#ifndef KINETIC_HELPER_H_
#define KINETIC_HELPER_H_
#include "metadata_info.h"
#include "database.pb.h"

/* Metadata */
int get_metadata    (const std::shared_ptr<MetadataInfo> &mdi);
int create_metadata (const std::shared_ptr<MetadataInfo> &mdi);
int delete_metadata (const std::shared_ptr<MetadataInfo> &mdi); // version mismatch -> -EAGAIN
int put_metadata    (const std::shared_ptr<MetadataInfo> &mdi); // version mismatch -> -EAGAIN

// forced put_metadata uses supplied md_update function on remote metadata in case on version mismatch.
// E.g. updating path permissions. Never return -EAGAIN.
int put_metadata_forced(const std::shared_ptr<MetadataInfo> &mdi, std::function<void()> md_update);


/* Data */
int get_data    (const std::string &key, std::shared_ptr<DataInfo> &di);
int put_data    (const std::shared_ptr<DataInfo> &di);          // will always resolve version miss-match using incremental update
int delete_data (const std::shared_ptr<DataInfo> &di);          // ignores version

/* Database */
int put_db_entry    (std::int64_t version, const posixok::db_entry &entry);
int get_db_entry    (std::int64_t version, posixok::db_entry &entry);
int get_db_version  (std::int64_t &version);

#endif
