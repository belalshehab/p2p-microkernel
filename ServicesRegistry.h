//
// Created by Belal Shehab on 21/02/2026.
//

#ifndef MICROKERNEL_SERVICESREGISTERY_H
#define MICROKERNEL_SERVICESREGISTERY_H

#include "ServiceHandle.h"
#include <map>
#include <string>

class ServicesRegistry {
public:
    [[nodiscard]] ServiceHandle* registerService(const char *serviceName, const char *binary_path);

    bool unregisterService(const std::string &name);

    void unregisterAllServices();

    inline ServiceHandle &getService(const std::string &name) {
        return m_services.at(name);
    }

private:
    std::map<std::string, ServiceHandle> m_services;
};


#endif //MICROKERNEL_SERVICESREGISTERY_H
