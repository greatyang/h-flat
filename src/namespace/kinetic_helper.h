#ifndef KINETIC_HELPER_H_
#define KINETIC_HELPER_H_
#include "metadata_info.h"
#include "database.pb.h"

/* Metadata */
int get_metadata    (MetadataInfo *mdi);
int put_metadata    (MetadataInfo *mdi); // in case of version missmatch, will attempt to merge metadata updates
int create_metadata (MetadataInfo *mdi); // fails on   version missmatch
int delete_metadata (MetadataInfo *mdi); // fails on   version missmatch

/* Data */
int get_data    (MetadataInfo *mdi, unsigned int blocknumber); // set DataInfo in mdi
int put_data    (MetadataInfo *mdi, unsigned int blocknumber); // in case of version missmatch, will resolve using incremental update
int delete_data (MetadataInfo *mdi, unsigned int blocknumber); // ignores version

/* Database */
int put_db_entry    (std::int64_t version, const posixok::db_entry &entry);
int get_db_entry    (std::int64_t version, posixok::db_entry &entry);
int get_db_version  (std::int64_t &version);

#endif
