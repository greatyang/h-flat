#ifndef KINETIC_NAMESPACE_H_
#define KINETIC_NAMESPACE_H_

#include "flat_namespace.h"
#include "kinetic/connection_handle.h"
#include <memory>


/*
 * Kinetic namespace. Simple one drive / one connection version for now, needs to be extended to be useful in real life.
 */
class KineticNamespace final : public FlatNamespace {
private:
	std::unique_ptr<kinetic::ConnectionHandle> con;
	const std::string db_basename    	 = "pathmapDB_";
	const std::string db_versionname	 = "pathmapDB_version";

private:
	NamespaceStatus updateDBVersionKey (std::int64_t version);

public:
	/* Metadata */
	NamespaceStatus getMD( 	 MetadataInfo *mdi);
	NamespaceStatus putMD (  MetadataInfo *mdi);
	NamespaceStatus deleteMD(MetadataInfo *mdi);

	/* Data */
	NamespaceStatus get(	 MetadataInfo *mdi, unsigned int blocknumber, std::string &value);
	NamespaceStatus put(	 MetadataInfo *mdi, unsigned int blocknumber, const std::string &value, const PutModeType type );
	NamespaceStatus free(    MetadataInfo *mdi, unsigned int blocknumber);

	/* Database Handling */
	NamespaceStatus getDBEntry( 	std::int64_t version, posixok::db_entry &entry);
	NamespaceStatus putDBEntry(		std::int64_t version, const posixok::db_entry &entry);
	NamespaceStatus getDBVersion( 	std::int64_t &version);

public:
	explicit KineticNamespace();
	~KineticNamespace();
};


#endif /* KINETIC_NAMESPACE_H_ */
