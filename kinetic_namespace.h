#ifndef KINETIC_NAMESPACE_H_
#define KINETIC_NAMESPACE_H_

#include "flat_namespace.h"
#include <memory>

#include "kinetic/kinetic_connection_factory.h"
#include "kinetic_connection.h"
#include "value_factory.h"
#include "hmac_provider.h"


/*
 * Kinetic namespace. Simple one drive / one connection version for now, needs to be extended to be useful in real life.
 */

class KineticNamespace final : public FlatNamespace {

private:
	com::seagate::kinetic::HmacProvider 			 hmac_provider;
    com::seagate::kinetic::ValueFactory 			 value_factory;

    std::unique_ptr<kinetic::KineticConnectionFactory> con_factory;
	std::unique_ptr<kinetic::KineticConnection> connection;

private:
	void fixDBVersionMissmatch(std::int64_t version);

public:
	/* Metadata */
	NamespaceStatus getMD( MetadataInfo *mdi);
	NamespaceStatus putMD( MetadataInfo *mdi);
	NamespaceStatus deleteMD(MetadataInfo *mdi);

	/* Data */
	NamespaceStatus get(	MetadataInfo *mdi, unsigned int blocknumber, std::string *value);
	NamespaceStatus put(	MetadataInfo *mdi, unsigned int blocknumber, const std::string &value);
	NamespaceStatus append(	MetadataInfo *mdi, const std::string &value);
	NamespaceStatus free(   MetadataInfo *mdi, unsigned int blocknumber);

	/* Database Handling */
	NamespaceStatus putDBEntry(		std::int64_t version, const posixok::db_entry &entry);
	NamespaceStatus getDBEntry( 	std::int64_t version, posixok::db_entry &entry);
	NamespaceStatus getDBVersion( 	std::int64_t &version);

public:
	explicit KineticNamespace();
	~KineticNamespace();
};


#endif /* KINETIC_NAMESPACE_H_ */
