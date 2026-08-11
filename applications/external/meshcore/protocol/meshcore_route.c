#include "meshcore_route.h"

MeshCoreCodeSet meshcore_code_set_one(uint8_t code) {
    MeshCoreCodeSet set = {{code}, 1};
    return set;
}

MeshCoreCodeSet meshcore_code_set(const uint8_t* codes, uint8_t count) {
    MeshCoreCodeSet set = {{0}, 0};
    if(count > MESHCORE_CODE_SET_MAX) count = MESHCORE_CODE_SET_MAX;
    for(uint8_t i = 0; i < count; i++) set.codes[i] = codes[i];
    set.count = count;
    return set;
}

bool meshcore_code_set_has(const MeshCoreCodeSet* set, uint8_t code) {
    for(uint8_t i = 0; i < set->count; i++) {
        if(set->codes[i] == code) return true;
    }
    return false;
}

MeshCoreRoute meshcore_route_event(
    bool pending,
    const MeshCoreCodeSet* want,
    bool has_stream,
    uint8_t ev_code) {
    /* Nothing asked for anything: everything is unsolicited. This is the case
     * that matters for the messenger — a MSG_WAITING push arriving while the
     * user is idle must still reach the application. */
    if(!pending) return MeshCoreRouteEvent;

    /* An awaited code finishes the request. Checked before the error case so
     * that a request which genuinely awaits ERR still works. */
    if(meshcore_code_set_has(want, ev_code)) return MeshCoreRouteReply;

    /* A node-side error ends any request; the caller gets to see the code and
     * tell it apart from silence. */
    if(ev_code == MC_RESP_ERR) return MeshCoreRouteReply;

    /* Multi-frame replies (GET_CONTACTS and friends) are offered to the
     * collector, which declines anything that is not part of its stream — a
     * push that lands mid-stream then falls through to the application
     * instead of being swallowed. */
    if(has_stream) return MeshCoreRouteStream;

    return MeshCoreRouteEvent;
}
