//
// Created by bartosz on 08.06.16.
//

#ifndef SERVER_ZEROCONF_HPP
#define SERVER_ZEROCONF_HPP

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
#include <avahi-client/lookup.h>
#include <avahi-common/thread-watch.h>


#include <boost/thread/mutex.hpp>
#include <boost/bimap/bimap.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

namespace zeroconf {

    class ZeroconfService
    {
    public:
        std::string name;
        std::string type;
        int port;
        std::string domain;
        std::string hostname;
        std::string description;
        std::string cookie;
        bool is_local;
        bool our_own;
        bool wide_area;
        bool multicast;
        bool cached;
        int protocol;
        int interface;
        std::vector<std::string> ipv4_addresses;
        std::vector<std::string> ipv6_addresses;
    };

    struct ZeroconfServiceCompare
    {
        bool operator()(const ZeroconfService &a, const ZeroconfService &b) const
        {
            if (a.name != b.name)
            {
                return a.name < b.name;
            }
            else if (a.type != b.type)
            {
                return a.type < b.type;
            }
            else
            {
                return a.port < b.port;
            }
        }
    };

    class DiscoveredZeroconfService
    {
    public:
        DiscoveredZeroconfService() :
                resolver(NULL)
        {
        }
        DiscoveredZeroconfService(ZeroconfService &discovered_service, AvahiServiceResolver *new_resolver,
                                  int protocol, int hardware_interface) :
                service(discovered_service), protocol(protocol), hardware_interface(hardware_interface), resolver(new_resolver)
        {

        }
        ~DiscoveredZeroconfService()
        {
            if (resolver)
            {
                avahi_service_resolver_free(resolver);
            }
        }
        ZeroconfService service;
        int protocol;
        int hardware_interface;
        AvahiServiceResolver *resolver;
    };

    struct DiscoveredZeroconfServiceCompare
    {
        bool operator()(const boost::shared_ptr<DiscoveredZeroconfService> avahi_service_a,
                        const boost::shared_ptr<DiscoveredZeroconfService> avahi_service_b) const
        {
            const ZeroconfService &a = avahi_service_a->service;
            const ZeroconfService &b = avahi_service_b->service;
            if (a.name != b.name)
            {
                return a.name < b.name;
            }
            else if (a.type != b.type)
            {
                return a.type < b.type;
            }
            else if (a.domain != b.domain)
            {
                return a.domain < b.domain;
            }
            else if (avahi_service_a->hardware_interface != avahi_service_b->hardware_interface)
            {
                return avahi_service_a->hardware_interface < avahi_service_b->hardware_interface;
            }
            else
            {
                return avahi_service_a->protocol < avahi_service_b->protocol;
            }
        }
    };


    class Zeroconf {
    private:
        typedef boost::bimaps::bimap<AvahiEntryGroup*, boost::bimaps::set_of<ZeroconfService, ZeroconfServiceCompare> > service_bimap;
        typedef boost::bimaps::bimap<AvahiServiceBrowser*, boost::bimaps::set_of<std::string> > discovery_bimap;
        typedef std::set<boost::shared_ptr<DiscoveredZeroconfService>, DiscoveredZeroconfServiceCompare> discovered_service_set;
        typedef std::pair<AvahiEntryGroup*, ZeroconfService> service_map_pair;
        typedef boost::function<void(ZeroconfService)> connection_signal_cb;

        service_bimap committed_services;
        service_bimap established_services;
        discovery_bimap discovery_service_types;
        discovered_service_set discovered_services;
        boost::mutex service_mutex;

        bool invalid_object;
        AvahiThreadedPoll *threaded_poll;
        AvahiClient *client;
        const int interface;
        const int permitted_protocols;

        connection_signal_cb new_connection_signal, lost_connection_signal;


        static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata);
        static void client_callback(AvahiClient *c, AvahiClientState state, void * userdata);
        static void discovery_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol,
                                       AvahiBrowserEvent event, const char *name, const char *type, const char *domain,
                                       AvahiLookupResultFlags flags, void* userdata);
        static void resolve_callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol,
                                     AvahiResolverEvent event, const char *name, const char *type, const char *domain,
                                     const char *host_name, const AvahiAddress *address, uint16_t port, AvahiStringList *txt,
                                     AvahiLookupResultFlags flags, void* userdata);
        static void modify_callback(AVAHI_GCC_UNUSED AvahiTimeout *e, void *userdata);
        bool addServiceNonThreaded(ZeroconfService service);
        discovered_service_set::iterator findDiscoveredService(ZeroconfService &service);
        void fail()
        {
            avahi_threaded_poll_quit(threaded_poll);
            invalid_object = true;
        }
        int avahiToInetProtocol(const int &protocol);

    public:
        Zeroconf();
        ~Zeroconf();
        bool addService(ZeroconfService service);
        bool addListener(const std::string &type);
        bool removeService(const ZeroconfService &service);
        bool removeListener(const std::string &type);
        bool isAlive() const { return !invalid_object;}
        void listDiscoveredServices(const std::string &service_type, std::vector<ZeroconfService> &list);
        void listPublishedServices(const std::string &service_type, std::vector<ZeroconfService> &list);

        void connect_signal_callbacks(connection_signal_cb new_connections, connection_signal_cb lost_connections)
        {
            new_connection_signal = new_connections;
            lost_connection_signal = lost_connections;
        }

        void spin();
    };
}

#endif //SERVER_ZEROCONF_HPP
