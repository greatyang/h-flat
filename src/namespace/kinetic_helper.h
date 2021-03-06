/* h-flat file system: Hierarchical Functionality in a Flat Namespace
 * Copyright (c) 2014 Seagate
 * Written by Paul Hermann Lensing
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
int put_db_entry    (std::int64_t version, const hflat::db_entry &entry);
int get_db_entry    (std::int64_t version, hflat::db_entry &entry);
int get_db_version  (std::int64_t &version);

int put_db_snapshot (const hflat::db_snapshot &s);
int get_db_snapshot (hflat::db_snapshot &s);

#endif
