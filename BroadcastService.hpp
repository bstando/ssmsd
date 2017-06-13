//
// Created by bartosz on 06.06.16.
//

#ifndef SERVER_BROADCASTSERVICE_HPP
#define SERVER_BROADCASTSERVICE_HPP


#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
#include <avahi-client/lookup.h>
#include <avahi-common/thread-watch.h>

#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cassert>


class BroadcastService {
public:
    AvahiThreadedPoll *threadedPoll;
    AvahiClient *client;


    static void create_services(AvahiClient *c);
    static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata);
    static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata);
    static void modify_callback(AVAHI_GCC_UNUSED AvahiTimeout *e, void *userdata);
    void start();
};


#endif //SERVER_BROADCASTSERVICE_HPP
