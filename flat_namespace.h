#ifndef FLAT_NAMESPACE_H_
#define FLAT_NAMESPACE_H_

#include "metadata_info.h"
#include "kinetic/status.h"
/*
 * Let`s just pretend not to see the use of KineticStatus in this class.
 */

class NamespaceStatus final : public kinetic::KineticStatus
{
public:
      static NamespaceStatus makeInvalid() {
    	  NamespaceStatus ret("Invalid value");
          ret.invalid_value_ = true;
          return ret;
      }

      NamespaceStatus(std::string error_message)
          : kinetic::KineticStatus(error_message), invalid_value_(false){
      }
      NamespaceStatus(kinetic::KineticStatus kinetic)
      	  : kinetic::KineticStatus(kinetic), invalid_value_(false){
      }
      NamespaceStatus(kinetic::Status status)
      	  : kinetic::KineticStatus(status), invalid_value_(false){
      }


      bool notValid() const {
          return invalid_value_;
      }

  private:
      bool invalid_value_;
};

class FlatNamespace{
public:
	virtual NamespaceStatus getMD( MetadataInfo *const mdi) = 0;
	virtual NamespaceStatus putMD( MetadataInfo *const mdi) = 0;
	virtual NamespaceStatus deleteMD( MetadataInfo *const mdi ) = 0;

	virtual NamespaceStatus get(	MetadataInfo *mdi, unsigned int blocknumber, std::string *value) = 0;
	virtual NamespaceStatus put(	MetadataInfo *mdi, unsigned int blocknumber, const std::string &value) = 0;
	virtual NamespaceStatus append(	MetadataInfo *mdi, const std::string &value) = 0;
	virtual NamespaceStatus free(   MetadataInfo *mdi, unsigned int blocknumber) = 0;

public:
	virtual ~FlatNamespace(){};
};


#endif /* FLAT_NAMESPACE_H_ */
