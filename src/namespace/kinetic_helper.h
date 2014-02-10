#ifndef KINETIC_HELPER_H_
#define KINETIC_HELPER_H_
#include "metadata_info.h"
#include "database.pb.h"

/* Metadata */
int get_metadata    (const std::shared_ptr<MetadataInfo> &mdi);
int put_metadata    (const std::shared_ptr<MetadataInfo> &mdi); // in case of version missmatch, attempt to merge metadata updates but can fail with -EAGAIN
int create_metadata (const std::shared_ptr<MetadataInfo> &mdi); // fails on   version missmatch
int delete_metadata (const std::shared_ptr<MetadataInfo> &mdi); // fails on   version missmatch

/* Data */
int get_data    (const std::string &key, std::shared_ptr<DataInfo> &di);
int put_data    (const std::shared_ptr<DataInfo> &di);          // in case of version missmatch, will always resolve using incremental update
int delete_data (const std::shared_ptr<DataInfo> &di);          // ignores version

/* Database */
int put_db_entry    (std::int64_t version, const posixok::db_entry &entry);
int get_db_entry    (std::int64_t version, posixok::db_entry &entry);
int get_db_version  (std::int64_t &version);

#endif
