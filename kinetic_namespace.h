#ifndef KINETIC_NAMESPACE_H_
#define KINETIC_NAMESPACE_H_

#include "flat_namespace.h"
#include <memory>

#include "kinetic_connection.h"
#include "hmac_provider.h"
#include "value_factory.h"

/*
 * Kinetic namespace. Simple one drive / one connection version for now, needs to be extended to be useful in real life.
 */

class KineticNamespace final : public FlatNamespace {

private:
	com::seagate::kinetic::HmacProvider 		hmac_provider;
	com::seagate::kinetic::ValueFactory 		value_factory;
	std::unique_ptr<kinetic::KineticConnection> connection;

public:
	NamespaceStatus getMD( MetadataInfo *mdi);
	NamespaceStatus putMD( MetadataInfo *mdi);
	NamespaceStatus deleteMD(MetadataInfo *const mdi);

	NamespaceStatus get(	MetadataInfo *mdi, unsigned int blocknumber, std::string *value);
	NamespaceStatus put(	MetadataInfo *mdi, unsigned int blocknumber, const std::string &value);
	NamespaceStatus append(	MetadataInfo *mdi, const std::string &value);
	NamespaceStatus free(   MetadataInfo *mdi, unsigned int blocknumber);

public:
	explicit KineticNamespace();
	~KineticNamespace();
};


#endif /* KINETIC_NAMESPACE_H_ */
