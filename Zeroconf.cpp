//
// Created by bartosz on 08.06.16.
//


#include "Zeroconf.hpp"

#include <avahi-common/alternative.h>
#include <avahi-common/timeval.h>
#include <avahi-common/thread-watch.h>
#include <boost/thread/thread.hpp>
#include <iostream>
#include <sys/socket.h>
#include <boost/log/trivial.hpp>

namespace zeroconf {
    bool Zeroconf::AddService(zeroconf::ZeroconfService service) {
        avahi_threaded_poll_lock(threaded_poll);
        bool result = addServiceNonThreaded(service);
        avahi_threaded_poll_unlock(threaded_poll);
        return result;
    }

    bool Zeroconf::AddListener(const std::string &type) {
        if (invalid_object) {
            //TODO: Write to log
            //std::cerr << "Invalid object Error" << std::endl;
            BOOST_LOG_TRIVIAL(fatal) << "Invalid object Error";
            return false;
        }

        {
            boost::mutex::scoped_lock lock(service_mutex);
            discovery_bimap::right_iterator browser_iter = discovery_service_types.right.find(type);
            if (browser_iter != discovery_service_types.right.end()) {
                //TODO: Write to log
                //std::cout << "NON empty" << std::endl;
                BOOST_LOG_TRIVIAL(fatal) << "NON empty";
                return false;
            }
        }
        AvahiServiceBrowser *serviceBrowser = NULL;
        avahi_threaded_poll_lock(threaded_poll);
        if (!(serviceBrowser = avahi_service_browser_new(client, interface, permitted_protocols, type.c_str(), NULL,
                                                         static_cast<AvahiLookupFlags>(0), discovery_callback, this))) {
            //TODO: Write to log
            //std::cerr << "Service Browser couldn't start" << std::endl;
            BOOST_LOG_TRIVIAL(fatal) << "Service Browser couldn't start";
            return false;
        }
        avahi_threaded_poll_unlock(threaded_poll);
        {
            boost::mutex::scoped_lock lock(service_mutex);
            discovery_service_types.insert(discovery_bimap::value_type(serviceBrowser, type));
        }
        return true;
    }

    bool Zeroconf::RemoveService(const ZeroconfService &service) {

        AvahiEntryGroup *group = NULL;
        bool erased = false;
        {
            boost::mutex::scoped_lock lock(service_mutex);
            service_bimap::right_const_iterator iter = established_services.right.find(service);
            if (iter != established_services.right.end()) {
                group = iter->second;
                established_services.right.erase(service);
                erased = true;
                BOOST_LOG_TRIVIAL(info) << "Zeroconf: removing service [" << service.name << "][" << service.type
                                        << "]";
            } else {
                BOOST_LOG_TRIVIAL(error) <<
                                         "Zeroconf: couldn't remove not currently advertised service [" << service.name
                                         << "][" << service.type << "]";
            }
        }
        if (group) {
            avahi_threaded_poll_lock(threaded_poll);
            avahi_entry_group_reset(group);
            avahi_entry_group_free(group);
            avahi_threaded_poll_unlock(threaded_poll);
        }
        return erased;
    }

    bool Zeroconf::RemoveListener(const std::string &type) {
        AvahiServiceBrowser *service_browser = NULL;

        /* Check if we're already listening for it. */
        {
            boost::mutex::scoped_lock lock(service_mutex);
            discovery_bimap::right_iterator browser_iter = discovery_service_types.right.find(type);
            if (browser_iter == discovery_service_types.right.end()) {
                //TODO: Write to log
                BOOST_LOG_TRIVIAL(fatal) << "Zeroconf : not currently listening for '" << type
                                         << "', aborting listener removal.";
                return false;
            } else {
                //TODO: Write to log
                service_browser = browser_iter->second;
                // delete internal browser pointers and storage
                discovery_service_types.right.erase(browser_iter);
                // delete internally resolved list
                discovered_service_set::iterator iter = discovered_services.begin();
                while (iter != discovered_services.end()) {
                    if ((*iter)->service.type == type) {
                        //TODO: Write to log
                        discovered_services.erase(iter++);
                    } else {
                        //TODO: Write to log
                        ++iter;
                    }
                }
            }
        }
        /* Remove the avahi browser */
        if (service_browser) {
            avahi_threaded_poll_lock(threaded_poll);
            avahi_service_browser_free(service_browser);
            avahi_threaded_poll_unlock(threaded_poll);
        }
        return true;
    }


    void Zeroconf::entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {

        Zeroconf *zeroconf = static_cast<Zeroconf *>(userdata);
        switch (state) {
            case AVAHI_ENTRY_GROUP_ESTABLISHED: {
                ZeroconfService service;
                {
                    boost::mutex::scoped_lock lock(zeroconf->service_mutex);
                    service_bimap::left_const_iterator left = zeroconf->committed_services.left.find(g);
                    if (left != zeroconf->committed_services.left.end()) {
                        service = left->second;
                    } else {

                        BOOST_LOG_TRIVIAL(fatal)
                            << "Zeroconf : should never reach here, please report a bug in zeroconf_avahi's entry_group_callback.";
                        return;
                    }
                    zeroconf->established_services.insert(service_bimap::value_type(g, service));
                    zeroconf->committed_services.left.erase(g);
                }

                BOOST_LOG_TRIVIAL(info) << "Zeroconf: service successfully established [" << service.name << "]["
                                        << service.type << "][" << service.port << "]";
                break;
            }
            case AVAHI_ENTRY_GROUP_COLLISION: {
                /* A service name collision with a 'remote' service happened. Let's pick a new name */
                ZeroconfService service;
                {
                    boost::mutex::scoped_lock lock(zeroconf->service_mutex);
                    service_bimap::left_const_iterator left = zeroconf->committed_services.left.find(g);
                    if (left != zeroconf->committed_services.left.end()) {
                        service = left->second;
                    } else {

                        BOOST_LOG_TRIVIAL(fatal)
                            << "Zeroconf : should never reach here, please report a bug in zeroconf_avahi's entry_group_callback.";
                        return;
                    }
                    zeroconf->committed_services.left.erase(g);
                }
                std::string alternative_name = avahi_alternative_service_name(service.name.c_str());
                BOOST_LOG_TRIVIAL(error) <<
                                         "Zeroconf: service name collision, renaming service [" << service.name << "]"
                                         << "][" << alternative_name << "]";
                service.name = alternative_name;
                avahi_entry_group_free(g);
                /* And recreate the services - already in the poll thread, so don' tneed to lock. */
                zeroconf->addServiceNonThreaded(service);
                break;
            }

            case AVAHI_ENTRY_GROUP_FAILURE:
                /* Some kind of failure happened while we were registering our services */
                // drop our committed_service here.
                BOOST_LOG_TRIVIAL(fatal) <<
                                         "Zeroconf: group state changed, system failure when trying to register service ["
                                         << avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))) << "]";
                avahi_entry_group_free(g);
                zeroconf->fail();
                break;

            case AVAHI_ENTRY_GROUP_UNCOMMITED: {
                BOOST_LOG_TRIVIAL(info) << "Zeroconf: group state changed, service uncommitted";
                // This is just mid-process, no need to handle committed_services
                break;
            }
            case AVAHI_ENTRY_GROUP_REGISTERING: {
                // This is just mid-process, no need to handle committed_services
                ZeroconfService service;
                {
                    boost::mutex::scoped_lock lock(zeroconf->service_mutex);
                    service_bimap::left_const_iterator left = zeroconf->committed_services.left.find(g);
                    if (left != zeroconf->committed_services.left.end()) {
                        service = left->second;
                    } else {
                        BOOST_LOG_TRIVIAL(fatal) <<
                                                 "Zeroconf : should never reach here, please report a bug in zeroconf_avahi's entry_group_callback.";
                        return;
                    }
                }
                BOOST_LOG_TRIVIAL(info) <<
                                        "Zeroconf: group state changed, service registering [" << service.name << "]["
                                        << service.type << "]";
                break;
            }
            default: {
                BOOST_LOG_TRIVIAL(info) << "Zeroconf: group state changed, ended in an unknown state [" << state << "]";
                break;
            }
        }

    }

    void Zeroconf::client_callback(AvahiClient *c, AvahiClientState state, void *userdata) {

        Zeroconf *zeroconf = static_cast<Zeroconf *>(userdata);
        assert(c);

        /* Called whenever the client or server state changes */

        switch (state) {
            case AVAHI_CLIENT_S_RUNNING: {
                /* The server has startup successfully and registered its host
                 * name on the network, so it's time to fire up */
                BOOST_LOG_TRIVIAL(info) << "Zeroconf: avahi client up and running.";
                zeroconf->Spin();
                break;
            }
            case AVAHI_CLIENT_FAILURE: {
                BOOST_LOG_TRIVIAL(fatal) << "Zeroconf: avahi client failure [" << avahi_strerror(avahi_client_errno(c))
                                         << "]";
                zeroconf->fail();

                break;
            }
            case AVAHI_CLIENT_S_COLLISION: {
                BOOST_LOG_TRIVIAL(error) << "Zeroconf: avahi client collision.";
                /* Let's drop our registered services. When the server is back
                 * in AVAHI_SERVER_RUNNING state we will register them
                 * again with the new host name. */
                break;
            }
            case AVAHI_CLIENT_S_REGISTERING: {
                BOOST_LOG_TRIVIAL(info) << "Zeroconf: avahi client registering.";

                /* The server records are now being established. This
                 * might be caused by a host name change. We need to wait
                 * for our own records to register until the host name is
                 * properly established. */

                // official example resets the entry group here
                // since we have multiple groups, handling should be a bit more
                // complicated - ToDo
                break;
            }
            case AVAHI_CLIENT_CONNECTING: {
                BOOST_LOG_TRIVIAL(info) << "Zeroconf: avahi client connecting.";
                break;
            }
        }

    }

    void Zeroconf::discovery_callback(AvahiServiceBrowser *b, AvahiIfIndex interface, AvahiProtocol protocol,
                                      AvahiBrowserEvent event, const char *name, const char *type,
                                      const char *domain, AvahiLookupResultFlags flags, void *userdata) {
        Zeroconf *zeroconf = reinterpret_cast<Zeroconf *>(userdata);

        switch (event) {
            case AVAHI_BROWSER_FAILURE:
                BOOST_LOG_TRIVIAL(fatal) <<
                                         "Zeroconf: browser failure ["
                                         << avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b)));
                avahi_threaded_poll_quit(zeroconf->threaded_poll);
                return;

            case AVAHI_BROWSER_NEW: {
                ZeroconfService service;
                service.name = name;
                service.type = type;
                service.domain = domain;

                AvahiServiceResolver *resolver = avahi_service_resolver_new(zeroconf->client, interface, protocol, name,
                                                                            type,
                                                                            domain, zeroconf->permitted_protocols,
                                                                            static_cast<AvahiLookupFlags>(0),
                                                                            Zeroconf::resolve_callback, zeroconf);
                if (!resolver) {
                    BOOST_LOG_TRIVIAL(fatal) <<
                                             "Zeroconf: avahi resolver failure (avahi daemon problem) [" << name << "]["
                                             << avahi_strerror(avahi_client_errno(zeroconf->client)) << "]["
                                             << interface << "][" << (protocol) << "]";
                    break;
                }
                {
                    boost::mutex::scoped_lock lock(zeroconf->service_mutex);
                    boost::shared_ptr<DiscoveredZeroconfService> new_service(
                            new DiscoveredZeroconfService(service, resolver, interface,
                                                          zeroconf->avahiToInetProtocol(protocol)));
                    if ((zeroconf->discovered_services.insert(new_service)).second) {
                        BOOST_LOG_TRIVIAL(info) <<
                                                "Zeroconf: discovered new service [" << name << "][" << type << "]["
                                                << domain << "][" << interface << "][" << protocol << "]";
                        // we signal in the resolver, not here...though this might be a bad design
                        // decision if the connection is up and down alot.
                        // if ( zeroconf->new_connection_signal ) {
                        //     zeroconf->new_connection_signal(service);
                        // }
                    } else {
                        BOOST_LOG_TRIVIAL(fatal) <<
                                                 "Tried to insert a new service on top of an old stored one - probably a bug in zeroconf_avahi!";
                    }
                }
                break;
            }

            case AVAHI_BROWSER_REMOVE: {
                ZeroconfService service;
                service.name = name;
                service.type = type;
                service.domain = domain;
                {
                    boost::mutex::scoped_lock lock(zeroconf->service_mutex);
                    discovered_service_set::iterator iter = zeroconf->findDiscoveredService(service);
                    if (iter != zeroconf->discovered_services.end()) {
                        /*********************
                         ** Update
                         **********************/
                        zeroconf->discovered_services.erase(iter);
                        /*********************
                         ** Logging
                         **********************/
                        BOOST_LOG_TRIVIAL(info) <<
                                                "Zeroconf: service was removed [" << service.name << "]["
                                                << service.type << "][" << service.domain << "][" << interface << "]["
                                                << protocol << "]";
                        /*********************
                         ** Signal
                         **********************/
                        // we signal here...though this might get muddled if the connection is up/down alot
                        // I haven't road tested this much yet at all.
                        if (zeroconf->lost_connection_signal) {
                            zeroconf->lost_connection_signal(service);
                        }
                    } else {
                        BOOST_LOG_TRIVIAL(fatal) <<
                                                 "Zeroconf: attempted to remove a non-discovered service (probably a bug in zeroconf_avahi!)";
                    }
                }
                break;
            }
            case AVAHI_BROWSER_ALL_FOR_NOW:
            case AVAHI_BROWSER_CACHE_EXHAUSTED: {
                if (event == AVAHI_BROWSER_CACHE_EXHAUSTED) {
                    BOOST_LOG_TRIVIAL(info) << "Zeroconf: browser event occured [cache exhausted]" << std::endl;
                } else {
                    BOOST_LOG_TRIVIAL(info) << "Zeroconf: browser event occured [all for now]" << std::endl;
                }
                break;
            }
        }


    }

    void Zeroconf::resolve_callback(AvahiServiceResolver *r, AvahiIfIndex interface, AvahiProtocol protocol,
                                    AvahiResolverEvent event, const char *name, const char *type,
                                    const char *domain, const char *host_name, const AvahiAddress *address,
                                    uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags,
                                    void *userdata) {

        Zeroconf *zeroconf = reinterpret_cast<Zeroconf *>(userdata);

        switch (event) {
            case AVAHI_RESOLVER_FAILURE: {
                ZeroconfService service;
                service.name = name;
                service.type = type;
                service.domain = domain;
                {
                    boost::mutex::scoped_lock lock(zeroconf->service_mutex);
                    discovered_service_set::iterator iter = zeroconf->findDiscoveredService(service);
                    if (iter != zeroconf->discovered_services.end()) {
                        /*********************
                         ** Logging
                         **********************/
                        if ((*iter)->service.ipv4_addresses.size() != 0) {
                            BOOST_LOG_TRIVIAL(error) <<
                                                     "Zeroconf: timed out resolving service [" << name << "][" << type
                                                     << "][" << domain << "][" << interface << "][" << protocol << "]["
                                                     << (*iter)->service.ipv4_addresses[0] << ":"
                                                     << (*iter)->service.port << "]";
                        } else if ((*iter)->service.ipv6_addresses.size() != 0) {
                            BOOST_LOG_TRIVIAL(error) <<
                                                     "Zeroconf: timed out resolving service [" << name << "][" << type
                                                     << "][" << domain << "][" << interface << "][" << protocol << "]["
                                                     << (*iter)->service.ipv6_addresses[0] << ":"
                                                     << (*iter)->service.port << "]";
                        } else {
                            BOOST_LOG_TRIVIAL(error) <<
                                                     "Zeroconf: timed out resolving service [" << name << "][" << type
                                                     << "][" << domain << "][" << interface << "][" << protocol << "]";
                        }
                        /*********************
                         ** Update
                         **********************/
                        (*iter)->service.ipv4_addresses.clear();
                        (*iter)->service.ipv6_addresses.clear();
                        (*iter)->service.hostname = "";
                        (*iter)->service.port = 0;
                        // could reset all the other stuff too, but the above is important.
                        /*********************
                         ** Signals
                         **********************/
                        if (zeroconf->lost_connection_signal) {
                            zeroconf->lost_connection_signal(service);
                        }
                    } else {
                        BOOST_LOG_TRIVIAL(error) <<
                                                 "Zeroconf: timed out resolving a service that was not saved, probably a zeroconf_avahi bug!";
                    }
                }
                break;
            }
            case AVAHI_RESOLVER_FOUND: {
                char a[AVAHI_ADDRESS_STR_MAX], *t;

                // workaround for avahi bug 1) above
                boost::this_thread::sleep(boost::posix_time::milliseconds(500));

                t = avahi_string_list_to_string(txt);
                avahi_address_snprint(a, sizeof(a), address);

                ZeroconfService service;
                service.name = name;
                service.type = type;
                service.domain = domain;
                bool error = false;
                switch (protocol) {
                    case (AVAHI_PROTO_INET): {
                        service.ipv4_addresses.push_back(a);
                        // workaround for avahi bug 2) above
                        size_t found = std::string(a).find(":");
                        if (found != std::string::npos) {
                            BOOST_LOG_TRIVIAL(fatal) <<
                                                     "Zeroconf: avahi is behaving badly (bug) - set an ipv6 address for an ipv4 service, recovering...";
                            avahi_free(t);
                            error = true;
                            break;
                        }
                        break;
                    }
                    case (AVAHI_PROTO_INET6): {
                        service.ipv6_addresses.push_back(a);
                        break;
                    }
                    default:
                        BOOST_LOG_TRIVIAL(fatal) << "Some fatal error during resolve!!!";
                        break; // should never get here
                }
                if (error) {
                    break;
                }

                service.hostname = host_name;
                service.port = port;
                service.description = t;
                service.cookie = avahi_string_list_get_service_cookie(txt);
                service.is_local = ((flags & AVAHI_LOOKUP_RESULT_LOCAL) == 0 ? false : true);
                service.our_own = ((flags & AVAHI_LOOKUP_RESULT_OUR_OWN) == 0 ? false : true);
                service.wide_area = ((flags & AVAHI_LOOKUP_RESULT_WIDE_AREA) == 0 ? false : true);
                service.multicast = ((flags & AVAHI_LOOKUP_RESULT_MULTICAST) == 0 ? false : true);
                service.cached = ((flags & AVAHI_LOOKUP_RESULT_CACHED) == 0 ? false : true);
                {
                    boost::mutex::scoped_lock lock(zeroconf->service_mutex);
                    discovered_service_set::iterator iter = zeroconf->findDiscoveredService(service);
                    if (iter != zeroconf->discovered_services.end()) {
                        /*********************
                         ** Update service info
                         **********************/
                        (*iter)->service = service;

                        (*iter)->protocol = zeroconf->avahiToInetProtocol(protocol);

                        /*********************
                         ** Logging
                         **********************/
                        BOOST_LOG_TRIVIAL(info) <<
                                                "Zeroconf: resolved service [" << name << "][" << type << "][" << domain
                                                << "][" << interface << "][" << ((*iter)->protocol) << "][" << a << ":"
                                                << service.port << "]";
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tname: " << service.name;
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \ttype: " << service.type;
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tdomain: " << service.domain;
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tinterface: " << interface;
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tprotocol: " << ((*iter)->protocol);
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \thostname: " << service.hostname;
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \taddress: " << a;
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tport: " << service.port;
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tdescription: " << service.description;
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tcookie: " << service.cookie;
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tis_local: " << (service.is_local ? 1 : 0);
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tour_own: " << (service.our_own ? 1 : 0);
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \twide_area: " << (service.wide_area ? 1 : 0);
                        BOOST_LOG_TRIVIAL(info) << "Zeroconf: \tmulticast: " << (service.multicast ? 1 : 0);
                        /*********************
                         ** Signals
                         **********************/
                        if (zeroconf->new_connection_signal) {
                            zeroconf->new_connection_signal(service);
                        }
                    } else {
                        BOOST_LOG_TRIVIAL(fatal) <<
                                                 "Zeroconf: timed out resolving a service that was not saved, probably a zeroconf_avahi bug!";
                    }
                }

                avahi_free(t);
                break;
            }
        }


    }

    void Zeroconf::modify_callback(AvahiTimeout *e, void *userdata) {

    }

    Zeroconf::Zeroconf() : invalid_object(false), threaded_poll(NULL), client(NULL), interface(AVAHI_IF_UNSPEC),
                           permitted_protocols(AVAHI_PROTO_INET) {

        int error;
        if (!(threaded_poll = avahi_threaded_poll_new())) {
            //LOG ERROR
            invalid_object = true;
            return;
        }
        client = avahi_client_new(avahi_threaded_poll_get(threaded_poll), static_cast<AvahiClientFlags>(0),
                                  Zeroconf::client_callback, this, &error);
        if (!client) {
            //LOG ERROR
            invalid_object = true;
            return;
        }
    }

    Zeroconf::~Zeroconf() {
        boost::mutex::scoped_lock(service_mutex);
        for (discovery_bimap::left_const_iterator iter = discovery_service_types.left.begin();
             iter != discovery_service_types.left.end(); ++iter) {
            avahi_service_browser_free(iter->first);
        }
        discovered_services.clear();
        discovery_service_types.clear();
        if (threaded_poll) {
            avahi_threaded_poll_stop(threaded_poll);
        }
        if (client) {
            avahi_client_free(client);
        }
        if (threaded_poll) {
            avahi_threaded_poll_free(threaded_poll);
        }
    }

    void Zeroconf::Spin() {

        if (!invalid_object) {
            //TODO: Write to log
            BOOST_LOG_TRIVIAL(info) << "Starting Zeroconf";

            avahi_threaded_poll_start(threaded_poll);
        }
    }

    void Zeroconf::ListDiscoveredServices(const std::string &service_type, std::vector<ZeroconfService> &list) {
        list.clear();
        boost::mutex::scoped_lock lock(service_mutex);
        if (service_type == "") {
            for (discovered_service_set::iterator iter = discovered_services.begin();
                 iter != discovered_services.end(); ++iter) {
                // ignore services that aren't currently resolved
                if (((*iter)->service.ipv4_addresses.size() != 0) || ((*iter)->service.ipv6_addresses.size() != 0)) {
                    list.push_back((*iter)->service);
                }
            }
        } else {
            for (discovered_service_set::iterator iter = discovered_services.begin();
                 iter != discovered_services.end(); ++iter) {
                if ((*iter)->service.type == service_type) {
                    // ignore services that aren't currently resolved
                    if (((*iter)->service.ipv4_addresses.size() != 0) ||
                        ((*iter)->service.ipv6_addresses.size() != 0)) {
                        list.push_back((*iter)->service);
                    }
                }
            }
        }
    }

    void Zeroconf::ListPublishedServices(const std::string &service_type,
                                         std::vector<ZeroconfService> &list) {
        list.clear();
        boost::mutex::scoped_lock lock(service_mutex);
        if (service_type == "") {
            for (service_bimap::left_const_iterator iter = established_services.left.begin();
                 iter != established_services.left.end(); ++iter) {
                list.push_back(iter->second);
            }
        } else {
            for (service_bimap::left_const_iterator iter = established_services.left.begin();
                 iter != established_services.left.end(); ++iter) {
                if (iter->second.type == service_type) {
                    list.push_back(iter->second);
                }
            }
        }

    }


    bool Zeroconf::addServiceNonThreaded(ZeroconfService service) {
        /*
  * We may still be initialising (from constructor)...check that we're up and running."
  */
        if (avahi_client_get_state(client) != AVAHI_CLIENT_S_RUNNING) {
            //TODO: Write to log
            BOOST_LOG_TRIVIAL(fatal) << "Client NOT Running";
            return false;
        }

        /*
         * Check we're not already publishing this exact service
         */
        {
            boost::mutex::scoped_lock lock(service_mutex);
            service_bimap::right_const_iterator iter;
            iter = committed_services.right.find(service);
            if (iter != committed_services.right.end()) {
                //TODO: Write to log
                BOOST_LOG_TRIVIAL(info) << "Service already commited";
                return true;
            }
            iter = established_services.right.find(service);
            if (iter != established_services.right.end()) {
                //TODO: Write to log
                BOOST_LOG_TRIVIAL(info) << "Service already established";
                return true;
            }
        }

        //TODO: Write to log
        /* If this is the first time we're called, let's create a new
         * entry group if necessary */
        AvahiEntryGroup *group = NULL;
        if (!(group = avahi_entry_group_new(client, entry_group_callback, this))) {
            //TODO: Write to log
            BOOST_LOG_TRIVIAL(error) << "Error while adding new group";
            fail();
            return false;
        }

        int ret = avahi_entry_group_add_service(group, interface, permitted_protocols,
                                                static_cast<AvahiPublishFlags>(0), // AVAHI_PUBLISH_USE_MULTICAST - we don't seem to need this, but others said they have needed it.
                                                service.name.c_str(), service.type.c_str(), service.domain.c_str(),
                                                NULL, // automatically sets a hostname for us
                                                static_cast<uint16_t>(service.port), NULL); // txt description
        if (ret < 0) {
            avahi_entry_group_free(group);
            if (ret == AVAHI_ERR_COLLISION) {
                std::string old_name = service.name;
                ZeroconfService new_service = service;
                service.name = avahi_alternative_service_name(service.name.c_str());
                //TODO: Write to log
                BOOST_LOG_TRIVIAL(error) << "Collison names";
                return addServiceNonThreaded(service);
            } else {
                BOOST_LOG_TRIVIAL(fatal) << "Very bad error durin adding service";
                //TODO: Write to log
                fail();
                return false;
            }
        }
        {
            boost::mutex::scoped_lock lock(service_mutex);
            committed_services.insert(service_bimap::value_type(group, service)); // should check the return value...
        }
        if ((ret = avahi_entry_group_commit(group)) < 0) {
            BOOST_LOG_TRIVIAL(error) << "Zeroconf: failed to commit entry group [" << avahi_strerror(ret) << "]";
            avahi_entry_group_free(group);
            {
                boost::mutex::scoped_lock lock(service_mutex);
                committed_services.left.erase(group);
            }
            fail();
            return false;
        }
        BOOST_LOG_TRIVIAL(info) << "Zeroconf: service committed, waiting for callback...";
        return true;
    }

    Zeroconf::discovered_service_set::iterator Zeroconf::findDiscoveredService(ZeroconfService &service) {
        discovered_service_set::iterator iter = discovered_services.begin();
        while (iter != discovered_services.end()) {
            if (((*iter)->service.name == service.name) && ((*iter)->service.type == service.type)
                && ((*iter)->service.domain == service.domain)) {
                return iter;
            } else {
                ++iter;
            }
        }
        return discovered_services.end();
    }

    int Zeroconf::avahiToInetProtocol(const int &protocol) {
        switch (protocol) {
            case AVAHI_PROTO_INET:
                return AF_INET;
                break;
            case AVAHI_PROTO_INET6:
                return AF_INET6;
                break;
            case AVAHI_PROTO_UNSPEC:
                return AF_UNSPEC;
                break;
            default:
                return AF_UNSPEC;
                break;
        }
    }


}














