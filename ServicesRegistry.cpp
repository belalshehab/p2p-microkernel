//
// Created by Belal Shehab on 21/02/2026.
//

#include "ServicesRegistry.h"

ServiceHandle *ServicesRegistry::registerService(const char *serviceName, const char *binary_path) {
    ServiceHandle service;
    if (!service.init(serviceName, binary_path)) {
        std::cerr << "[Orchestrator] Failed to create service: " << serviceName << "\n";
        return nullptr;
    }
    auto [itr, _] = m_services.emplace(serviceName, std::move(service));
    return &itr->second;
}

bool ServicesRegistry::unregisterService(const std::string &name) {
    if (!m_services.contains(name)) {
        std::cout << "[" << name << "] is not Registerd\n";
        return false;
    }
    m_services[name].closeServiceHandle();
    m_services.erase(name);
    return true;
}

void ServicesRegistry::unregisterAllServices() {
    m_services.clear();
}
