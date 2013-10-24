/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * Implementation of the VCookieStore that utilize Couchbase to store the
 * objects in.
 *
 * @author Trond Norbye
 */

#include "VCCouchbaseStore.h"
#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <cassert>

extern "C" {
    static void error_handler(lcb_t inst, lcb_error_t err, const char *info) {
        std::cerr << "FATAL: We received an error: "
                  << lcb_strerror(inst, err);
        if (info) {
            std::cerr << " " << info;
        }
        std::cerr << std::endl
                  << "       TODO: add recovery. " << std::endl;

        exit(EXIT_FAILURE);
    }

    static void store_handler(lcb_t instance, const void *cookie,
                              lcb_storage_t operation,
                              lcb_error_t error,
                              const lcb_store_resp_t *resp)
    {
        bool *retval = (bool*)cookie;
        if (error == LCB_SUCCESS) {
            *retval = true;
        } else {
            std::cerr << "Failed to store object: "
                      << lcb_strerror(instance, error) << std::endl;
            *retval = false;
        }
    }

    struct get_cookie {
        std::vector<char> buffer;
        lcb_error_t error;
    };

    static void get_handler(lcb_t instance, const void *cookie,
                            lcb_error_t error,
                            const lcb_get_resp_t *resp)
    {
        get_cookie *gc = (get_cookie*)cookie;
        gc->error = error;
        if (error == LCB_SUCCESS) {
            gc->buffer.resize(resp->v.v0.nbytes);
            std::memcpy(&gc->buffer[0], (char*)resp->v.v0.bytes,
                        resp->v.v0.nbytes);
        }
    }

    static void remove_handler(lcb_t instance, const void *cookie,
                               lcb_error_t error,
                               const lcb_remove_resp_t *)
    {
        bool *retval = (bool*)cookie;
        if (error == LCB_SUCCESS) {
            *retval = true;
        } else {
            std::cerr << "Failed to remove object: "
                      << lcb_strerror(instance, error) << std::endl;
            *retval = false;
        }
    }
}

VCCouchbaseStore::VCCouchbaseStore()
{
    lcb_create_st options("10.46.20.12:8091", NULL, NULL, "default");
    lcb_error_t error = lcb_create(&instance, &options);
    if (error != LCB_SUCCESS) {
        std::cerr << "Failed to create instance: "
                  << lcb_strerror(NULL, error) << std::endl;
        exit(EXIT_FAILURE);
    }
    lcb_set_error_callback(instance, error_handler);
    lcb_set_store_callback(instance, store_handler);
    lcb_set_get_callback(instance, get_handler);
    lcb_set_remove_callback(instance, remove_handler);
    lcb_behavior_set_syncmode(instance, LCB_SYNCHRONOUS);

    if ((error = lcb_connect(instance)) != LCB_SUCCESS) {
        std::cerr << "Failed to connect to cluster: "
                  << lcb_strerror(NULL, error) << std::endl;
    }
}

VCCouchbaseStore::~VCCouchbaseStore()
{
    lcb_destroy(instance);
}

bool VCCouchbaseStore::SaveVCookie(VCookie const &vcookie)
{
    std::vector<char> buffer;
    Serialize(vcookie, buffer);

    std::stringstream ss;
    ss << vcookie.GetUser();
    std::string key = ss.str();

    lcb_store_cmd_t cmd(LCB_SET,
                        key.data(), key.length(),
                        &buffer[0], buffer.size());
    const lcb_store_cmd_t * const commands[] = { &cmd };
    bool retval;
    lcb_error_t error = lcb_store(instance, &retval, 1, commands);
    if (error != LCB_SUCCESS) {
        std::cerr << "Failed to store item: "
                  << lcb_strerror(instance, error) << std::endl;
        return false;
    }

    return retval;
}

bool VCCouchbaseStore::LoadVCookie(VCookie &vcookie)
{
    std::stringstream ss;
    ss << vcookie.GetUser();
    std::string key = ss.str();

    lcb_get_cmd_t cmd(key.data(), key.length());
    const lcb_get_cmd_t * const commands[] = { &cmd };
    struct get_cookie gc;
    lcb_error_t error = lcb_get(instance, &gc, 1, commands);
    if (error != LCB_SUCCESS) {
        std::cerr << "Failed to get item: "
                  << lcb_strerror(instance, error) << std::endl;
        return false;
    }

    if (gc.error == LCB_SUCCESS) {
        Deserialize(vcookie, gc.buffer);
        return true;
    } else {
        if (gc.error != LCB_KEY_ENOENT) {
            std::cerr << "Failed to get item: "
                      << lcb_strerror(instance, gc.error) << std::endl;
        }
        return false;
    }
}

bool VCCouchbaseStore::DeleteVCookie(VCookie &vcookie)
{
    std::stringstream ss;
    ss << vcookie.GetUser();
    std::string key = ss.str();

    lcb_remove_cmd_t cmd(key.data(), key.length());
    const lcb_remove_cmd_t * const commands[] = { &cmd };
    bool retval;
    lcb_error_t error = lcb_remove(instance, &retval, 1, commands);
    if (error != LCB_SUCCESS) {
        std::cerr << "Failed to remove item: "
                  << lcb_strerror(instance, error) << std::endl;
        return false;
    }

    return retval;
}

struct bulk_cookie {
    std::map<std::string, VCookie*> objects;
    VCookieProcessedCallback callback;
};

extern "C" {
    static void bulk_get_handler(lcb_t instance, const void *cookie,
                                 lcb_error_t status,
                                 const lcb_get_resp_t *resp)
    {
        std::string key((const char*)resp->v.v0.key, resp->v.v0.nkey);
        bulk_cookie *gc = (bulk_cookie*)cookie;
        VCookie *obj = gc->objects[key];
        assert(obj != NULL);

        if (status == LCB_SUCCESS) {
            std::vector<char> buffer;
            buffer.resize(resp->v.v0.nbytes);
            std::memcpy(&buffer[0], (const char*)resp->v.v0.bytes,
                        resp->v.v0.nbytes);
            VCookieStore::Deserialize(*obj, buffer);
        }

        if (gc->callback != NULL) {
            gc->callback((status == LCB_SUCCESS), *obj);
        }
    }

    static void bulk_store_handler(lcb_t instance, const void *cookie,
                                   lcb_storage_t operation,
                                   lcb_error_t error,
                                   const lcb_store_resp_t *resp)
    {
        bool *retval = (bool*)cookie;
        if (error == LCB_SUCCESS) {
            *retval = true;
        } else {
            std::cerr << "Failed to store object: "
                      << lcb_strerror(instance, error) << std::endl;
            *retval = false;
        }
    }
}

void VCCouchbaseStore::LoadVCookies(std::vector<VCookie*> &cookies,
                                    VCookieProcessedCallback callback)
{
    lcb_behavior_set_syncmode(instance, LCB_ASYNCHRONOUS);
    lcb_set_get_callback(instance, bulk_get_handler);

    struct bulk_cookie bc;
    bc.callback = callback;
    lcb_get_cmd_t *cmds;
    lcb_get_cmd_t **commands;

    cmds = (lcb_get_cmd_t*)calloc(cookies.size(), sizeof(lcb_get_cmd_t));
    commands = (lcb_get_cmd_t**)calloc(cookies.size(), sizeof(lcb_get_cmd_t*));

    std::vector<VCookie*>::iterator ii;
    int idx;
    for (idx = 0, ii = cookies.begin(); ii != cookies.end(); ++ii, ++idx) {
        VCookie *cookie = *ii;
        std::stringstream ss;
        ss << cookie->GetUser();
        std::string key = ss.str();
        bc.objects[key] = cookie;
        cmds[idx].v.v0.key = key.data();
        cmds[idx].v.v0.nkey = key.length();
        commands[idx] = cmds + idx;
    }

    lcb_error_t error = lcb_get(instance, &bc, cookies.size(), commands);
    assert(error == LCB_SUCCESS);

    lcb_wait(instance);
    free(cmds);
    free(commands);
    lcb_behavior_set_syncmode(instance, LCB_SYNCHRONOUS);
    lcb_set_get_callback(instance, get_handler);
}

void VCCouchbaseStore::SaveVCookies(std::vector<const VCookie*> &cookies,
                                    VCookieProcessedCallback callback)
{
    lcb_behavior_set_syncmode(instance, LCB_ASYNCHRONOUS);
    lcb_set_store_callback(instance, bulk_store_handler);

    struct bulk_cookie bc;
    bc.callback = callback;
    std::vector<const VCookie*>::iterator ii;
    for (ii = cookies.begin(); ii != cookies.end(); ++ii) {
        const VCookie *cookie = *ii;
        std::stringstream ss;
        ss << cookie->GetUser();
        std::string key = ss.str();
        bc.objects[key] = const_cast<VCookie*>(cookie);

        std::vector<char> buffer;
        Serialize(*cookie, buffer);

        lcb_store_cmd_t cmd(LCB_SET, key.data(), key.length(),
                            &buffer[0], buffer.size());

        const lcb_store_cmd_t * const commands[] = { &cmd };
        bool retval;
        lcb_error_t error = lcb_store(instance, &retval, 1, commands);
        assert(error == LCB_SUCCESS);
    }

    lcb_wait(instance);
    lcb_behavior_set_syncmode(instance, LCB_SYNCHRONOUS);
    lcb_set_store_callback(instance, store_handler);
}

unsigned long long VCCouchbaseStore::DeleteOldVCookies(time_t t)
{
    return 0;
}

unsigned long long VCCouchbaseStore::GetVCookieCount() const
{
    return 0;
}

bool VCCouchbaseStore::GetVCookie(VCookie &vcookie,
                                          unsigned long long index) const
{
        return false;
}
