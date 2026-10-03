#include "mf_radio_tx_session.h"

#include <string.h>

static bool mf_radio_tx_session_ops_valid(const MfRadioHardwareOps* ops) {
    return ops != NULL && ops->prepare_tx != NULL && ops->prepare_carrier_rx != NULL &&
           ops->set_tx_level != NULL && ops->stop_tx != NULL && ops->read_carrier != NULL &&
           ops->read_rssi_dbm != NULL && ops->frequency_valid != NULL && ops->tx_allowed != NULL &&
           ops->default_frequency != NULL && ops->idle != NULL && ops->sleep != NULL;
}

void mf_radio_tx_session_init(MfRadioTxSession* session, const MfRadioHardwareOps* hardware) {
    if(session == NULL) return;
    memset(session, 0, sizeof(*session));
    session->hardware = hardware;
}

bool mf_radio_tx_session_frequency_allowed(const MfRadioTxSession* session, uint32_t frequency_hz) {
    return session != NULL && mf_radio_tx_session_ops_valid(session->hardware) &&
           session->hardware->frequency_valid(session->hardware->context, frequency_hz) &&
           session->hardware->tx_allowed(session->hardware->context, frequency_hz);
}

bool mf_radio_tx_session_prepare(
    MfRadioTxSession* session,
    uint32_t frequency_hz,
    MfRadioTxMode mode) {
    if(session == NULL || !mf_radio_tx_session_frequency_allowed(session, frequency_hz))
        return false;
    if(session->prepared) mf_radio_tx_session_stop(session);
    if(!session->hardware->prepare_tx(session->hardware->context, frequency_hz, mode)) {
        session->hardware->stop_tx(session->hardware->context);
        session->hardware->idle(session->hardware->context);
        session->hardware->sleep(session->hardware->context);
        return false;
    }
    session->prepared = true;
    return true;
}

bool mf_radio_tx_session_start(
    MfRadioTxSession* session,
    uint32_t frequency_hz,
    MfRadioTxMode mode) {
    if(!mf_radio_tx_session_prepare(session, frequency_hz, mode)) return false;
    if(mode == MfRadioTxModeCwfm &&
       !session->hardware->set_tx_level(session->hardware->context, false)) {
        mf_radio_tx_session_stop(session);
        return false;
    }
    return true;
}

bool mf_radio_tx_session_set_mark(MfRadioTxSession* session, bool mark) {
    if(session == NULL || !session->prepared) return !mark;
    if(session->mark == mark) return true;
    if(!session->hardware->set_tx_level(session->hardware->context, mark)) {
        mf_radio_tx_session_stop(session);
        return false;
    }
    session->mark = mark;
    return true;
}

void mf_radio_tx_session_stop(MfRadioTxSession* session) {
    if(session == NULL) return;
    if(session->hardware != NULL && mf_radio_tx_session_ops_valid(session->hardware)) {
        session->hardware->stop_tx(session->hardware->context);
        session->hardware->idle(session->hardware->context);
        session->hardware->sleep(session->hardware->context);
    }
    session->prepared = false;
    session->mark = false;
}
