//
//  WCL.cpp
//  AirportItlwm
//
//  Created by laobamac on 2026/10/1.
//  Copyright © 2026 laobamac. All rights reserved.
//

#ifdef AIRPORT_WCL
#include "../AirportItlwmV2.hpp"
#include "../AirportItlwmSkywalkInterface.hpp"
#include <kern/thread_call.h>

#include <kern/clock.h>
#include <net80211/ieee80211_node.h>
#include <net80211/ieee80211_ioctl.h>
#include <net80211/ieee80211_priv.h>

extern IOCommandGate *_fCommandGate;

struct AirportItlwmSkywalkInterface::CountryUpdate {
    AirportItlwmSkywalkInterface *interface;
    AirportItlwm *controller;
    IOCommandGate *gate;
    IO80211WorkQueue *queue;
    thread_call_t call;
};

void AirportItlwmSkywalkInterface::deferWCLCountryUpdate()
{
    auto queue = instance->getWorkQueue();
    if (wclStopping || !_fCommandGate || !queue)
        return;
    auto update = static_cast<CountryUpdate *>(IOMalloc(sizeof(CountryUpdate)));
    if (!update)
        return;
    update->call = thread_call_allocate(wclCountryUpdateThread, update);
    if (!update->call) {
        IOFree(update, sizeof(*update));
        return;
    }
    update->interface = this;
    update->controller = instance;
    update->gate = _fCommandGate;
    update->queue = queue;
    retain();
    instance->retain();
    update->gate->retain();
    queue->retain();
    thread_call_enter(update->call);
}

void AirportItlwmSkywalkInterface::wclCountryUpdateThread(void *arg, void *)
{
    auto update = static_cast<CountryUpdate *>(arg);
    // Native country notifications synchronously query WCL. Holding the gate
    // is necessary, but executing on the driver's workloop thread is forbidden.
    if (!update->interface->wclStopping)
        update->gate->runAction(wclCountryUpdateGated, update);
    thread_call_free(update->call);
    update->interface->release();
    update->controller->release();
    update->gate->release();
    update->queue->release();
    IOFree(update, sizeof(*update));
}

struct AirportItlwmSkywalkInterface::WCLDeferredTask {
    AirportItlwmSkywalkInterface *interface;
    AirportItlwm *controller;
    IOCommandGate *gate;
    IO80211WorkQueue *queue;
    thread_call_t call;
    uint32_t action;
    bool force;
};

// Hand a piece of work over to the driver work queue.
//
// The receive interrupt context may neither call into IO80211Family nor block
// on the command gate: the family asserts that the interface is only touched
// from its own work queue thread ("trying to send on thread panic"
// @IO80211Glue.cpp:417) and the gate can sleep.
void AirportItlwmSkywalkInterface::deferWCLAction(uint32_t action, bool force)
{
    auto queue = instance ? instance->getWorkQueue() : nullptr;
    if (wclStopping || !_fCommandGate || !queue)
        return;
    auto task = static_cast<WCLDeferredTask *>(IOMalloc(sizeof(WCLDeferredTask)));
    if (!task)
        return;
    task->call = thread_call_allocate(wclDeferredThread, task);
    if (!task->call) {
        IOFree(task, sizeof(*task));
        return;
    }
    task->interface = this;
    task->controller = instance;
    task->gate = _fCommandGate;
    task->queue = queue;
    task->action = action;
    task->force = force;
    retain();
    instance->retain();
    task->gate->retain();
    queue->retain();
    thread_call_enter(task->call);
}

void AirportItlwmSkywalkInterface::wclDeferredThread(void *arg, void *)
{
    auto task = static_cast<WCLDeferredTask *>(arg);
    if (!task->interface->wclStopping)
        task->gate->runAction(wclDeferredGated, task);
    thread_call_free(task->call);
    task->interface->release();
    task->controller->release();
    task->gate->release();
    task->queue->release();
    IOFree(task, sizeof(*task));
}

IOReturn AirportItlwmSkywalkInterface::wclDeferredGated(
    OSObject *, void *arg0, void *, void *, void *)
{
    auto task = static_cast<WCLDeferredTask *>(arg0);
    auto self = task->interface;
    if (self->wclStopping) {
        // Shutdown forensics: a deferred action that is still queued while the
        // interface is going down is a candidate for the shutdown stall.
        return kIOReturnNotReady;
    }

    switch (task->action) {
    case WCLDeferredLinkQuality: {
        // Already inside the gate, so the gated variant has to be called
        // directly: a nested runAction() on the same gate would deadlock.
        updateWCLLinkQualityGated(nullptr, self,
                                  reinterpret_cast<void *>(uintptr_t(task->force)),
                                  nullptr, nullptr);
        break;
    }
    case WCLDeferredHandshakeNotify:
    default: {
        // The legacy code path announces the finished handshake on the BSD
        // network interface (AirportSTAIOCTL.cpp:
        // getNetworkInterface()->postMessage). The WCL port has no such
        // interface, so the message goes out through the Skywalk interface,
        // which is where the framework listens.
        self->postMessage(APPLE80211_M_RSN_HANDSHAKE_DONE, nullptr, 0, false);
        self->postMessage(APPLE80211_M_LINK_CHANGED, nullptr, 0, false);
        self->postMessage(APPLE80211_M_BSSID_CHANGED, nullptr, 0, false);
        self->postMessage(APPLE80211_M_SSID_CHANGED, nullptr, 0, false);

        // The typed supplicant event is the message the framework actually
        // waits for. Its 802.1X state machine (WCL::axManager) turns it into
        // AX_MANAGER_EVENT_4WAYHS_DONE, which cancels the framework's
        // association timer; without it the framework leaves the network with
        // "4wayHsTimeout" ten seconds after the join.
        //
        // Both framework handlers require exactly 40 bytes and reject the
        // message unless the field at offset 8 is zero. It is sent from the
        // work queue because the framework writes a byte back into the
        // payload.
        AirportWCL::SupplicantEventMsg supplicant = {};
        supplicant.flags = 1;
        self->postMessage(AirportWCL::SupplicantEvent, &supplicant,
                          sizeof(supplicant), true);

        break;
    }
    }
    return kIOReturnSuccess;
}

IOReturn AirportItlwmSkywalkInterface::wclCountryUpdateGated(
    OSObject *, void *arg, void *, void *, void *)
{
    auto update = static_cast<CountryUpdate *>(arg);
    if (update->interface->wclStopping)
        return kIOReturnNotReady;
    if (!update->queue->inGate() || update->queue->onThread())
        return kIOReturnNotPermitted;
    update->interface->postMessage(APPLE80211_M_COUNTRY_CODE_CHANGED, nullptr, 0, false);
    return kIOReturnSuccess;
}

bool AirportItlwmSkywalkInterface::initWCL()
{
    joinTimer = IOTimerEventSource::timerEventSource(this, wclJoinTimeout);
    if (!joinTimer)
        return false;
    return instance->getWorkQueue()->addEventSource(joinTimer) == kIOReturnSuccess;
}

bool AirportItlwmSkywalkInterface::canUseCommandGate() const
{
    return !wclStopping && _fCommandGate;
}

void AirportItlwmSkywalkInterface::stopWCL()
{
    // The shutdown path must not block. Taking the command gate here waits for
    // whatever action is currently running, and stopWCLGated() used to run a
    // second action on the same non recursive gate, which deadlocked the
    // shutdown (the machine hung in "shutdown stall" reports).
    //
    // The marks below are the forensics for that: a "shutdown stall" report
    // only shows the kernel address the driver thread waits at, the marks show
    // how far the shutdown got.
    portAuthorized = false;
    wclLinkReady = false;
    wclStopping = true;
    if (joinTimer) {
        joinTimer->cancelTimeout();
        // Removing the event source waits for the work loop to finish the
        // event it is running. That work loop may be waiting for the command
        // gate, and the framework is tearing the interface down while it
        // waits, so this is the most likely place for the shutdown to hang.
        // The flag above is set before this point, so the event handler
        // returns at once and the wait cannot last.
        if (joinTimer->getWorkLoop())
            joinTimer->getWorkLoop()->removeEventSource(joinTimer);
        OSSafeReleaseNULL(joinTimer);
    }
    if (scanSource)
        scanSource->cancelTimeout();
    scanPending = joinPending = false;
    joinDeferred = false;
    explicit_bzero(&deferredJoin, sizeof(deferredJoin));
    if (fHalService) {
        ieee80211com *ic = fHalService->get80211Controller();
        ic->ic_wcl_join_requested = false;
        ic->ic_wcl_scan_requested = false;
        explicit_bzero(ic->ic_psk, sizeof(ic->ic_psk));
    }
}

IOReturn AirportItlwmSkywalkInterface::stopWCLGated(
    OSObject *, void *arg, void *, void *, void *)
{
    auto self = static_cast<AirportItlwmSkywalkInterface *>(arg);
    self->portAuthorized = false;
    self->wclLinkReady = false;
    // The link quality update runs inside the gate already, so call the gated
    // variant directly. updateWCLLinkQuality() would start a nested action on
    // the same gate and deadlock.
    updateWCLLinkQualityGated(nullptr, self,
                              reinterpret_cast<void *>(uintptr_t(true)), nullptr, nullptr);
    self->wclStopping = true;
    return kIOReturnSuccess;
}

void AirportItlwmSkywalkInterface::updateWCLLinkQuality(bool force)
{
    if (!_fCommandGate)
        return;
    // While the interface is going down the command gate may be held by the
    // framework, which in turn waits for the driver to finish. Asking for the
    // gate here then blocks the shutdown, so the update is simply dropped.
    if (!canUseCommandGate()) {
        return;
    }
    auto queue = instance ? instance->getWorkQueue() : nullptr;
    if (queue && queue->inGate()) {
        // Already inside the gate: run the action directly. A nested
        // runAction() on the same gate would deadlock.
        updateWCLLinkQualityGated(nullptr, this,
                                  reinterpret_cast<void *>(uintptr_t(force)), nullptr, nullptr);
        return;
    }
    _fCommandGate->runAction(updateWCLLinkQualityGated, this,
                             reinterpret_cast<void *>(uintptr_t(force)));
}

IOReturn AirportItlwmSkywalkInterface::updateWCLLinkQualityGated(
    OSObject *, void *arg0, void *arg1, void *, void *)
{
    auto self = static_cast<AirportItlwmSkywalkInterface *>(arg0);
    if (self->wclStopping || !self->fHalService)
        return kIOReturnNotReady;
    auto queue = self->instance->getWorkQueue();
    if (!queue || !queue->inGate())
        return kIOReturnNotPermitted;
    auto interface = self->mExpansionData ? self->getBSDInterface() : nullptr;
    if (!interface) {
        self->wclLinkQualityInterface = nullptr;
        self->wclReportedLinkQuality = -2;
        return kIOReturnNotReady;
    }
    auto ic = self->fHalService->get80211Controller();
    int quality = -2;
    if (self->wclLinkReady && self->linkState() == kIO80211NetworkLinkUp &&
        (ifnet_flags(interface) & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING) &&
        ic && ic->ic_state == IEEE80211_S_RUN && ic->ic_bss &&
        IEEE80211_ADDR_EQ(ic->ic_bss->ni_bssid, self->joinBSSID)) {
        int rssi = IWM_MIN_DBM + ic->ic_bss->ni_rssi;
        quality = rssi >= -70 ? 100 : (rssi >= -85 ? 50 : 20);
    }
    if (arg1 || quality != self->wclReportedLinkQuality ||
        interface != self->wclLinkQualityInterface) {
        self->setLQM(static_cast<unsigned long long>(static_cast<long long>(quality)));
        self->wclReportedLinkQuality = quality;
        self->wclLinkQualityInterface = interface;
    }
    return kIOReturnSuccess;
}

void AirportItlwmSkywalkInterface::free()
{
    if (joinTimer)
        stopWCL();
    IO80211InfraProtocol::free();
}

// Calls setLinkState() with the signature of the target version.
//
// The signature of setLinkState() has a fifth argument on Tahoe. That
// argument has no counterpart on Sequoia, so its meaning is unknown and
// zero is passed for it, which is the neutral value.
static bool wclSetLinkState(AirportItlwmSkywalkInterface *iface,
                             IO80211LinkState state, unsigned int generation,
                             bool debounce, unsigned int code)
{
#if __IO80211_TARGET == __MAC_15_2
    return iface->setLinkState(state, generation, debounce, code);
#else
    return iface->setLinkState(state, generation, debounce, code, 0);
#endif
}

IOReturn AirportItlwmSkywalkInterface::wclCommand(OSObject *, void *a0, void *a1, void *a2, void *)
{
    auto self = static_cast<AirportItlwmSkywalkInterface *>(a0);
    uintptr_t wcode = uintptr_t(a1);
    switch (wcode) {
        case 0: break;
        case 1: break;
        case 8: break;
        case 11: break;
        default: break;
    }
    if (self->wclStopping)
        return kIOReturnNotReady;
    switch (wcode) {
        case 13: return self->copyWCLChannels(a2);
        case 14: return self->copyWCLTrafficCounters(a2);
        case 15:
            return self->fHalService->setScanHomeAwayTime(
                AirportWCL::read32(static_cast<const uint8_t *>(a2)));
        case 12:
            self->handleWCLEvent(int(uintptr_t(a2)), nullptr);
            return kIOReturnSuccess;
        case 16:
            self->publishEarlyBssid();
            return kIOReturnSuccess;
        case 11: self->clearWCLPMKSA(); return kIOReturnSuccess;
        case 8: return self->installWCLKey(static_cast<apple80211_key *>(a2));
        case 9: return self->accessWCLRSN(static_cast<apple80211_rsn_ie_data *>(a2), true);
        case 10: return self->accessWCLRSN(static_cast<apple80211_rsn_ie_data *>(a2), false);
        case 0: return self->beginWCLScan(static_cast<apple80211ScanRequest *>(a2));
        case 1: return self->beginWCLJoin(static_cast<apple80211AssocCandidates *>(a2));
        case 2: return self->abortWCLJoin();
        case 3: return self->leaveWCLNetwork();
        case 6: return self->copyWCLExtendedBss(a2);
        case 7:
            if (!a2 || !self->wclBssValid || !self->wclAssociatedBeacons)
                return kIOReturnNotReady;
            memcpy(a2, self->wclBeaconInfo, sizeof(self->wclBeaconInfo));
            return kIOReturnSuccess;
        case 5:
            if (!a2)
                return kIOReturnBadArgument;
            if (!self->wclBssValid && self->wclBeaconMetaLen) {
                // The scan of the network may already be over when the
                // framework asks for its information. Answer with the last
                // beacon that was seen for it.
                memcpy(self->wclBssInfo, self->wclBeaconMeta, self->wclBeaconMetaLen);
                auto meta = reinterpret_cast<AirportWCL::BeaconMetadata *>(self->wclBssInfo);
                bool joinBSSIDKnown = false;
                for (unsigned i = 0; i < 6; ++i)
                    joinBSSIDKnown |= self->joinBSSID[i] != 0;
                if (joinBSSIDKnown)
                    memcpy(meta->bssid, self->joinBSSID, sizeof(meta->bssid));
                self->wclBssValid = true;
            }
            if (!self->wclBssValid)
                return kIOReturnNotReady;
            memcpy(a2, self->wclBssInfo, sizeof(self->wclBssInfo));
            return kIOReturnSuccess;
        case 4:
            self->finishWCLScan(kIOReturnAborted);
            return kIOReturnSuccess;
    }
    return kIOReturnUnsupported;
}

IOReturn AirportItlwmSkywalkInterface::setWCL_SCAN_REQ(apple80211ScanRequest *request)
{
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, nullptr, request);
}

IOReturn AirportItlwmSkywalkInterface::setWCL_SCAN_ABORT(void *)
{
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(4));
}

IOReturn AirportItlwmSkywalkInterface::setWCL_ASSOCIATE(apple80211AssocCandidates *request)
{
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(1), request);
}

#if __IO80211_TARGET == __MAC_15_2
IOReturn AirportItlwmSkywalkInterface::setWCL_JOIN_ABORT(void *)
#else
IOReturn AirportItlwmSkywalkInterface::setWCL_JOIN_ABORT(apple80211_wcl_abort_join *)
#endif
{
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(2));
}

IOReturn AirportItlwmSkywalkInterface::setWCL_LEAVE_NETWORK(apple80211_leave_network *)
{
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(3));
}

IOReturn AirportItlwmSkywalkInterface::beginWCLScan(apple80211ScanRequest *request)
{
    AirportWCL::ScanRequest scan;
    if (!AirportWCL::decodeScan(request, 0x1550, scan)) {
        return kIOReturnBadArgument;
    }
    ieee80211com *ic = fHalService->get80211Controller();
    if (!(ic->ic_if.if_flags & IFF_RUNNING))
        return kIOReturnNotReady;
    if (scanPending || joinPending || (ic->ic_flags & IEEE80211_F_BGSCAN))
        return kIOReturnBusy;
    if (ic->ic_state == IEEE80211_S_AUTH || ic->ic_state == IEEE80211_S_ASSOC)
        return kIOReturnBusy;
    if (scan.channelCount) {
        bool usable = false;
        for (unsigned channel = 1; channel <= IEEE80211_CHAN_MAX; ++channel)
            if (ic->ic_channels[channel].ic_flags &&
                AirportWCL::scanChannelSelected(scan.channels, true, channel))
                usable = true;
        if (!usable)
            return kIOReturnUnsupported;
    }
    scanPending = true;
    scanCompleted = false;
    scanSkipCompletion = ic->ic_state == IEEE80211_S_SCAN &&
        ((scan.type != 2 && scan.ssidLength != 0) ||
         ic->ic_wcl_scan_ssid_length != 0 || ic->ic_des_esslen != 0);
    ic->ic_wcl_scan_requested = true;
    ic->ic_wcl_scan_active = scan.type != 2;
    ic->ic_wcl_scan_ssid_length = scan.type == 2 ? 0 : scan.ssidLength;
    memcpy(ic->ic_wcl_scan_ssid, scan.ssid, sizeof(scan.ssid));
    ic->ic_wcl_scan_restricted = scan.channelCount != 0;
    {
        // A hidden network only shows up in the list when the system asks for
        // a directed scan of its name, so keep the request visible.
        char name[33] = { 0 };
        uint32_t nameLen = scan.ssidLength;
        if (nameLen > 32)
            nameLen = 32;
        memcpy(name, scan.ssid, nameLen);
    }
    memcpy(ic->ic_wcl_scan_channels, scan.channels, sizeof(scan.channels));
    scanSource->setTimeoutMS(20000);
    if (ic->ic_state == IEEE80211_S_RUN) {
        ieee80211_begin_cache_bgscan(&ic->ic_if);
        if (!(ic->ic_flags & IEEE80211_F_BGSCAN)) {
            scanPending = false;
            ic->ic_wcl_scan_requested = false;
            scanSource->cancelTimeout();
            return kIOReturnBusy;
        }
    } else if (ic->ic_state != IEEE80211_S_SCAN) {
        int error = ic->ic_newstate(ic, IEEE80211_S_SCAN, -1);
        if (error) {
            scanPending = false;
            ic->ic_wcl_scan_requested = false;
            scanSource->cancelTimeout();
            return kIOReturnError;
        }
    }
    return kIOReturnSuccess;
}

void AirportItlwmSkywalkInterface::finishWCLScan(IOReturn status)
{
    if (!scanPending)
        return;
    scanPending = false;
    scanCompleted = false;
    scanSource->cancelTimeout();
    fHalService->get80211Controller()->ic_wcl_scan_requested = false;
    instance->postMessage(this, AirportWCL::ScanDone, &status, sizeof(status), true);
}

void AirportItlwmSkywalkInterface::completeWCLScan()
{
    if (!wclStopping)
        finishWCLScan(scanCompleted ? kIOReturnSuccess : kIOReturnTimeout);
}

void AirportItlwmSkywalkInterface::publishWCLBeacon(const ieee80211_node *node, const uint8_t *ies, size_t length)
{
    if (!node || node->ni_esslen > 32 || !AirportWCL::validIEs(ies, length))
        return;
    ieee80211com *ic = fHalService->get80211Controller();
    if (!node->ni_chan || node->ni_chan == IEEE80211_CHAN_ANYC)
        return;
    unsigned channel = ieee80211_chan2ieee(ic, node->ni_chan);
    if (!channel || channel > 196)
        return;
    size_t size = sizeof(AirportWCL::BeaconMetadata) + length;
    auto metadata = static_cast<AirportWCL::BeaconMetadata *>(IOMalloc(size));
    if (!metadata)
        return;
    *metadata = {};
    metadata->ieLength = uint32_t(length);
    metadata->channelSpec = uint16_t(channel | (IEEE80211_IS_CHAN_5GHZ(node->ni_chan) ? 0xd000 : 0x1000));
    metadata->channel = channel;
    uint32_t ssidLength = node->ni_esslen;
    const uint8_t *ssid = node->ni_essid;
    if (ssidLength == 0 && wclHasHiddenSsid &&
        IEEE80211_ADDR_EQ(node->ni_bssid, wclHiddenBssid)) {
        // Hidden network: the beacon carries no name, but we know it from the
        // join request. Without the name the system neither lists the network
        // nor matches it against its preferred networks.
        ssidLength = wclHiddenSsidLength;
        ssid = wclHiddenSsid;
    }
    metadata->ssidLength = ssidLength;
    memcpy(metadata->ssid, ssid, ssidLength);
    memcpy(metadata->bssid, node->ni_bssid, 6);
    metadata->rssi = IWM_MIN_DBM + node->ni_rssi;
    metadata->interval = node->ni_intval;
    metadata->capability = node->ni_capinfo;
    metadata->flags = AirportWCL::SSIDPresent | AirportWCL::RSSIValid;
    memcpy(metadata + 1, ies, length);
    metadata->ieLength = uint32_t(AirportWCL::selectWPA2TransitionMode(
        reinterpret_cast<uint8_t *>(metadata + 1), length));
    size_t messageSize = sizeof(*metadata) + metadata->ieLength;
    // The framework asks the driver for the BSS information as soon as the
    // link comes up and it gives up on the network when that fails ("Update
    // Bss fail"). The information is taken from the beacons of the network,
    // so it has to be collected while the network is still being scanned:
    // waiting for the association to be complete is too late, the scan has
    // usually finished by then and no beacon arrives any more.
    //
    // Without the information the framework never runs setCurrentBSS, so
    // neither the BSSID nor the SSID reach the 802.1X supplicant, and the
    // join ends in "join failure".
    bool bssMatches = IEEE80211_ADDR_EQ(node->ni_bssid, joinBSSID);
    if (!bssMatches && wclHasHiddenSsid)
        bssMatches = IEEE80211_ADDR_EQ(node->ni_bssid, wclHiddenBssid);
    // Keep the beacon that was seen last. The framework asks for the BSS
    // information of the network when the link comes up, and the answer is
    // taken from here when the scan of that network is already over by then.
    if (messageSize <= sizeof(wclBeaconMeta)) {
        memcpy(wclBeaconMeta, metadata, messageSize);
        wclBeaconMetaLen = uint32_t(messageSize);
        memcpy(wclBeaconMetaBssid, node->ni_bssid, sizeof(wclBeaconMetaBssid));
    }
    if (bssMatches) {
        wclBssValid = messageSize <= sizeof(wclBssInfo);
        if (wclBssValid) {
            bzero(wclBssInfo, sizeof(wclBssInfo));
            memcpy(wclBssInfo, metadata, messageSize);
        }
    }
    if (joinPending && !assocNotified && ic->ic_des_esslen &&
        node->ni_esslen == ic->ic_des_esslen &&
        memcmp(node->ni_essid, ic->ic_des_essid, ic->ic_des_esslen) == 0) {
        bool nonzero = false;
        for (unsigned i = 0; i < 6; ++i)
            nonzero |= node->ni_bssid[i] != 0;
        if (nonzero) {
            memcpy(earlyBssid, node->ni_bssid, sizeof(earlyBssid));
            earlyBssidPending = true;
            if (wclHasHiddenSsid)
                memcpy(wclHiddenBssid, node->ni_bssid, sizeof(wclHiddenBssid));
            if (instance && instance->getWorkQueue() && instance->getWorkQueue()->inGate()) {
                publishEarlyBssid();
            } else if (_fCommandGate) {
                _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(16));
            }
        }
    }
    // A directed scan that was asked for by name teaches the driver the name
    // of the network it found. The beacons of a hidden network do not carry
    // that name, so without remembering it the scan results of that network
    // stay nameless and the system never lists it.
    if (ic->ic_wcl_scan_active && ic->ic_wcl_scan_ssid_length &&
        node->ni_esslen == ic->ic_wcl_scan_ssid_length &&
        memcmp(node->ni_essid, ic->ic_wcl_scan_ssid,
               ic->ic_wcl_scan_ssid_length) == 0) {
        bool nonzero = false;
        for (unsigned i = 0; i < 6; ++i)
            nonzero |= node->ni_bssid[i] != 0;
        if (nonzero) {
            memcpy(wclHiddenSsid, ic->ic_wcl_scan_ssid, ic->ic_wcl_scan_ssid_length);
            wclHiddenSsidLength = ic->ic_wcl_scan_ssid_length;
            wclHasHiddenSsid = true;
            memcpy(wclHiddenBssid, node->ni_bssid, sizeof(wclHiddenBssid));
        }
    }
    instance->postMessage(this, AirportWCL::ScanResult, metadata, messageSize, true);
    IOFree(metadata, size);
}

void AirportItlwmSkywalkInterface::publishEarlyBssid()
{
    if (wclStopping || !earlyBssidPending || !joinPending || assocNotified)
        return;
    bool nonzero = false;
    for (unsigned i = 0; i < 6; ++i)
        nonzero |= earlyBssid[i] != 0;
    if (!nonzero) {
        earlyBssidPending = false;
        return;
    }
    memcpy(joinBSSID, earlyBssid, sizeof(joinBSSID));
    setCurrentApAddress(reinterpret_cast<ether_addr *>(joinBSSID));
    earlyBssidPending = false;
}

void AirportItlwmSkywalkInterface::clearWCLPMKSA()
{
    auto ic = fHalService->get80211Controller();
    ieee80211_pmk *pmk;
    while ((pmk = TAILQ_FIRST(&ic->ic_pmksa))) {
        TAILQ_REMOVE(&ic->ic_pmksa, pmk, pmk_next);
        explicit_bzero(pmk, sizeof(*pmk));
        ::free(pmk);
    }
}

IOReturn AirportItlwmSkywalkInterface::setCLEAR_PMKSA_CACHE(void *)
{
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(11));
}

IOReturn AirportItlwmSkywalkInterface::installWCLKey(apple80211_key *key)
{
    static_assert(sizeof(apple80211_key) == 0x94, "WCL cipher key layout");
    if (!key)
        return kIOReturnBadArgument;
    auto ic = fHalService->get80211Controller();
    auto ni = ic->ic_bss;
    if (!ni || ic->ic_state != IEEE80211_S_RUN)
        return kIOReturnNotReady;

    if (key->key_cipher_type == APPLE80211_CIPHER_PMK ||
        key->key_cipher_type == APPLE80211_CIPHER_PMKSA ||
        key->key_cipher_type == APPLE80211_CIPHER_MSK) {
        // A zero length key is the "drop the cached PMK" request that precedes
        // the real key delivery; it is not an error.
        if (key->key_len == 0) {
            clearWCLPMKSA();
            return kIOReturnSuccess;
        }
        // EAP-PEAP hands over an MSK of 64 bytes while the PMK used by the
        // 802.1X AKM is its first 32 bytes, so a strict 32 byte check would
        // reject every enterprise key.
        if (key->key_len < IEEE80211_PMK_LEN || key->key_len > 64 ||
            key->key_rsc_len > 8) {
            return kIOReturnBadArgument;
        }
        enum ieee80211_akm akm = (enum ieee80211_akm)ni->ni_rsnakms;
        if (!ieee80211_is_8021x_akm(akm)) {
            akm = IEEE80211_AKM_8021X;
        }
        auto pmk = ieee80211_pmksa_add(ic, akm, ni->ni_macaddr, key->key, 0);
        return pmk ? kIOReturnSuccess : kIOReturnNoMemory;
    }

    if (key->key_cipher_type == APPLE80211_CIPHER_TKIP ||
        key->key_cipher_type == APPLE80211_CIPHER_AES_OCB ||
        key->key_cipher_type == APPLE80211_CIPHER_AES_CCM) {
        if (key->key_flags & APPLE80211_KEY_FLAG_UNICAST) {
            setPTK(key->key, key->key_len);
            instance->postMessage(this, APPLE80211_M_RSN_HANDSHAKE_DONE, nullptr, 0, false);
            return kIOReturnSuccess;
        }
        if ((key->key_flags & APPLE80211_KEY_FLAG_MULTICAST) || key->key_flags == 0) {
            setGTK(key->key, key->key_len, key->key_index, key->key_rsc);
            instance->postMessage(this, APPLE80211_M_RSN_HANDSHAKE_DONE, nullptr, 0, false);
            return kIOReturnSuccess;
        }
    }

    return kIOReturnUnsupported;
}

IOReturn AirportItlwmSkywalkInterface::accessWCLRSN(apple80211_rsn_ie_data *data, bool write)
{
    if (!data) return kIOReturnBadArgument;
    auto ic = fHalService->get80211Controller();
    if (write) {
        if (!AirportWCL::validRSNOverride(data->ie, data->len))
            return kIOReturnBadArgument;
        // net80211 constructs negotiated IEs for its own key handshake.
        return kIOReturnUnsupported;

    }
    const uint8_t *ie = ic->ic_bss ? ic->ic_bss->ni_rsnie : nullptr;
    if (!ie) return kIOReturnNotReady;
    size_t length = size_t(ie[1]) + 2;
    if (length > sizeof(data->ie)) return kIOReturnBadArgument;
    bzero(data, sizeof(*data));
    data->version = APPLE80211_VERSION;
    data->len = length;
    memcpy(data->ie, ie, length);
    return kIOReturnSuccess;
}

IOReturn AirportItlwmSkywalkInterface::beginWCLJoin(apple80211AssocCandidates *request)
{
    AirportWCL::JoinRequest join;
    if (!AirportWCL::decodeJoin(request, 0x2cc, join)) {
        return kIOReturnBadArgument;
    }
    return startWCLJoin(join);
}

IOReturn AirportItlwmSkywalkInterface::startWCLJoin(AirportWCL::JoinRequest &join)
{
    auto auth = AirportWCL::classifyLegacyAuth(join);
    bool personal = auth == AirportWCL::LegacyAuth::Personal;
    bool enterprise = auth == AirportWCL::LegacyAuth::Enterprise;
    bool supported = auth != AirportWCL::LegacyAuth::Invalid;
    ieee80211com *ic = fHalService->get80211Controller();
    wclLinkUpReported = false;
    // Remember the name of the network we are asked to join. Hidden networks
    // never broadcast it, so scan results of that BSSID would stay nameless.
    if (join.ssidLength && join.ssidLength <= sizeof(wclHiddenSsid)) {
        memcpy(wclHiddenSsid, join.ssid, join.ssidLength);
        wclHiddenSsidLength = join.ssidLength;
        wclHasHiddenSsid = true;
        memcpy(wclHiddenBssid, join.bssid, sizeof(wclHiddenBssid));
    }
    // The join request names the BSSID of the network. It is remembered right
    // away, before the join can be deferred: a private MAC change restarts
    // the firmware and the association only happens afterwards, but the 802.1X
    // supplicant asks for the BSSID of the network in the meantime and
    // refuses to send anything without it.
    bool bssidKnown = false;
    for (unsigned i = 0; i < 6; ++i)
        bssidKnown |= join.bssid[i] != 0;
    if (bssidKnown) {
        memcpy(ic->ic_des_bssid, join.bssid, 6);
        memcpy(joinBSSID, join.bssid, 6);
        ic->ic_flags |= IEEE80211_F_DESBSSID;
        // The 802.1X supplicant refuses to send anything while it does not
        // know the BSSID of the network, and it can receive the first EAP
        // request from the access point before the association is through.
        // The kernel side 802.1X state machine only passes the BSSID on when
        // the association is done, so the change is announced right away.
        instance->postMessage(this, APPLE80211_M_BSSID_CHANGED, nullptr, 0, false);
        instance->postMessage(this, APPLE80211_M_SSID_CHANGED, nullptr, 0, false);
    }
    // Applying a private MAC restarts firmware asynchronously. WCL can issue
    // the association before that restart has completed; keep one request
    // until the HAL reports READY rather than making the user retry it.
    if (supported && !joinPending && ic->ic_wcl_mac_reconfig &&
        (ic->ic_if.if_flags & IFF_UP)) {
        deferredJoin = join;
        memcpy(joinBSSID, join.bssid, sizeof(joinBSSID));
        explicit_bzero(&join, sizeof(join));
        joinDeferred = joinPending = true;
        joinTimer->setTimeoutMS(30000);
        return kIOReturnSuccess;
    }
    if (!supported || !(ic->ic_if.if_flags & IFF_RUNNING) || joinPending || scanPending) {
        explicit_bzero(&join, sizeof(join));
        return !supported ? kIOReturnUnsupported : kIOReturnBusy;
    }
    if (ic->ic_state == IEEE80211_S_RUN || ic->ic_state == IEEE80211_S_AUTH || ic->ic_state == IEEE80211_S_ASSOC)
        leaveWCLNetwork();
    ieee80211_disable_rsn(ic);
    ieee80211_disable_wep(ic);
    explicit_bzero(ic->ic_psk, sizeof(ic->ic_psk));
    clearWCLPMKSA();
    ic->ic_flags &= ~(IEEE80211_F_AUTO_JOIN | IEEE80211_F_PSK);
    ic->ic_wcl_enterprise = enterprise;

    if (personal || enterprise) {
        ieee80211_wpaparams wpa = {};
        wpa.i_enabled = 1;
        if (join.upperAuth & (APPLE80211_AUTHTYPE_WPA | APPLE80211_AUTHTYPE_WPA_PSK))
            wpa.i_protos |= IEEE80211_WPA_PROTO_WPA1;
        if (join.upperAuth & (APPLE80211_AUTHTYPE_WPA2 | APPLE80211_AUTHTYPE_WPA2_PSK |
                              APPLE80211_AUTHTYPE_SHA256_PSK | APPLE80211_AUTHTYPE_SHA256_8021X))
            wpa.i_protos |= IEEE80211_WPA_PROTO_WPA2;
        if (join.upperAuth & (APPLE80211_AUTHTYPE_WPA_PSK | APPLE80211_AUTHTYPE_WPA2_PSK))
            wpa.i_akms |= IEEE80211_WPA_AKM_PSK;
        if (join.upperAuth & (APPLE80211_AUTHTYPE_WPA | APPLE80211_AUTHTYPE_WPA2))
            wpa.i_akms |= IEEE80211_WPA_AKM_8021X;
        if (join.upperAuth & APPLE80211_AUTHTYPE_SHA256_PSK) {
            wpa.i_akms |= IEEE80211_WPA_AKM_SHA256_PSK;
            wpa.i_akms |= IEEE80211_WPA_AKM_PSK;
        }
        if (join.upperAuth & APPLE80211_AUTHTYPE_SHA256_8021X)
            wpa.i_akms |= IEEE80211_WPA_AKM_SHA256_8021X;
        wpa.i_ciphers = IEEE80211_WPA_CIPHER_CCMP | IEEE80211_WPA_CIPHER_TKIP;
        if (join.rsnLength >= 8 && join.rsn[0] == IEEE80211_ELEMID_RSN &&
            join.rsn[4] == 0 && join.rsn[5] == 0x0f && join.rsn[6] == 0xac) {
            if (join.rsn[7] == 2) wpa.i_groupcipher = IEEE80211_WPA_CIPHER_TKIP;
            if (join.rsn[7] == 4) wpa.i_groupcipher = IEEE80211_WPA_CIPHER_CCMP;
        }
        int error = ieee80211_ioctl_setwpaparms(ic, &wpa);
        if (error && error != ENETRESET) {
            explicit_bzero(&join, sizeof(join));
            return kIOReturnBadArgument;
        }
        if (personal) {
            memcpy(ic->ic_psk, join.key, sizeof(ic->ic_psk));
            ic->ic_flags |= IEEE80211_F_PSK;
        } else if (join.keyLength == IEEE80211_PMK_LEN) {
            bool cached = true;
            if (join.upperAuth & (APPLE80211_AUTHTYPE_WPA | APPLE80211_AUTHTYPE_WPA2))
                cached = ieee80211_pmksa_add(ic, IEEE80211_AKM_8021X, join.bssid, join.key, 0) != nullptr;
            if (cached && (join.upperAuth & APPLE80211_AUTHTYPE_SHA256_8021X))
                cached = ieee80211_pmksa_add(ic, IEEE80211_AKM_SHA256_8021X, join.bssid, join.key, 0) != nullptr;
            if (!cached) {
                clearWCLPMKSA();
                explicit_bzero(&join, sizeof(join));
                return kIOReturnNoMemory;
            }
        }
    } else if (auth == AirportWCL::LegacyAuth::Wep) {
        ieee80211_nwkey keys = {};
        keys.i_wepon = IEEE80211_NWKEY_WEP;
        keys.i_defkid = join.keyIndex + 1;
        keys.i_key[join.keyIndex].i_keylen = join.keyLength;
        keys.i_key[join.keyIndex].i_keydat = join.key;
        int error = ieee80211_ioctl_setnwkeys(ic, &keys);
        if (error && error != ENETRESET) {
            explicit_bzero(&join, sizeof(join));
            return kIOReturnBadArgument;
        }
    }
    ic->ic_des_esslen = join.ssidLength;
    memcpy(ic->ic_des_essid, join.ssid, sizeof(ic->ic_des_essid));
    if (bssidKnown) {
        ic->ic_wcl_scan_requested = false;
        setCurrentApAddress(reinterpret_cast<ether_addr *>(joinBSSID));
        if (enterprise) {
            // Safe here: the join request is handled in the context of the
            // framework's ioctl, so the interface is touched from the thread
            // IO80211Family expects.
            wclSetLinkState(this, kIO80211NetworkLinkUp, 0, false, 0);
            wclLinkUpReported = true;
        }
    } else {
        memset(ic->ic_des_bssid, 0, sizeof(ic->ic_des_bssid));
        memset(joinBSSID, 0, sizeof(joinBSSID));
        ic->ic_flags &= ~IEEE80211_F_DESBSSID;
        ic->ic_wcl_scan_requested = true;
        ic->ic_wcl_scan_active = true;
        ic->ic_wcl_scan_ssid_length = join.ssidLength;
        memcpy(ic->ic_wcl_scan_ssid, join.ssid, sizeof(ic->ic_wcl_scan_ssid));
        ic->ic_wcl_scan_restricted = false;
    }
    ic->ic_wcl_join_requested = true;
    current_authtype_lower = join.lowerAuth;
    current_authtype_upper = join.upperAuth;
    explicit_bzero(&join, sizeof(join));
    wclBssValid = false;
    earlyBssidPending = false;
    joinPending = true;
    assocNotified = firstBeaconNotified = portAuthorized = wclLinkReady = false;
    wclFirstBeaconHeld = false;
    wclAssociatedBeacons = 0;
    bzero(wclBeaconInfo, sizeof(wclBeaconInfo));
    disassocIsVoluntary = false;
    joinTimer->setTimeoutMS(30000);
    if (ic->ic_state != IEEE80211_S_SCAN && ic->ic_newstate(ic, IEEE80211_S_SCAN, -1)) {
        joinPending = false;
        joinTimer->cancelTimeout();
        ic->ic_wcl_join_requested = false;
        clearWCLPMKSA();
        ic->ic_wcl_enterprise = false;
        explicit_bzero(ic->ic_psk, sizeof(ic->ic_psk));
        return kIOReturnError;
    }
    return kIOReturnSuccess;
}

// WCL may assign the private MAC address only after the association, which
// restarts the firmware and drops the link. Keep the join parameters so the
// driver can join again once the firmware is ready.
void AirportItlwmSkywalkInterface::stashJoinForRejoin()
{
    auto ic = fHalService->get80211Controller();
    if (ic->ic_state != IEEE80211_S_RUN || ic->ic_des_esslen == 0 ||
        ic->ic_des_esslen > sizeof(ic->ic_des_essid))
        return;
    AirportWCL::JoinRequest join = {};
    join.lowerAuth = current_authtype_lower;
    join.upperAuth = current_authtype_upper;
    join.ssidLength = ic->ic_des_esslen;
    memcpy(join.ssid, ic->ic_des_essid, join.ssidLength);
    memcpy(join.bssid, joinBSSID, sizeof(join.bssid));
    deferredJoin = join;
    joinDeferred = true;
}

void AirportItlwmSkywalkInterface::resumeWCLJoin()
{
    if (!joinDeferred || wclStopping)
        return;
    AirportWCL::JoinRequest join = deferredJoin;
    explicit_bzero(&deferredJoin, sizeof(deferredJoin));
    joinDeferred = joinPending = false;
    joinTimer->cancelTimeout();
    IOReturn status = startWCLJoin(join);
    explicit_bzero(&join, sizeof(join));
    if (status != kIOReturnSuccess) {
        joinPending = true;
        finishWCLJoin(1, 0, status);
    }
}

void AirportItlwmSkywalkInterface::finishWCLJoin(uint16_t status, uint16_t reason, IOReturn driverStatus)
{
    if (!joinPending)
        return;
    AirportWCL::ConnectionEvent event = {};
    event.status = status;
    event.reason = reason;
    memcpy(event.peers[0].bssid, joinBSSID, 6);
    event.peers[0].driverStatus = driverStatus;
    joinPending = false;
    joinDeferred = false;
    explicit_bzero(&deferredJoin, sizeof(deferredJoin));
    joinTimer->cancelTimeout();
    instance->postMessage(this, AirportWCL::ConnectComplete, &event, sizeof(event), true);
    if (status) {
        portAuthorized = false;
        wclLinkReady = false;
        updateWCLLinkQuality(true);
        ieee80211com *ic = fHalService->get80211Controller();
        ic->ic_wcl_join_requested = false;
        clearWCLPMKSA();
        ic->ic_wcl_enterprise = false;
        explicit_bzero(ic->ic_psk, sizeof(ic->ic_psk));
    }
}

void AirportItlwmSkywalkInterface::wclJoinTimeout(OSObject *owner, IOTimerEventSource *)
{
    auto self = OSDynamicCast(AirportItlwmSkywalkInterface, owner);
    if (self && !self->wclStopping) {
        self->finishWCLJoin(1, 1004, kIOReturnTimeout);
        self->leaveWCLNetwork();
    }
}

IOReturn AirportItlwmSkywalkInterface::leaveWCLNetwork()
{
    ieee80211com *ic = fHalService->get80211Controller();
    portAuthorized = false;
    wclLinkReady = false;
    updateWCLLinkQuality(true);
    joinPending = false;
    joinDeferred = false;
    explicit_bzero(&deferredJoin, sizeof(deferredJoin));
    joinTimer->cancelTimeout();
    ic->ic_wcl_join_requested = false;
    wclAssociatedBeacons = 0;
    wclBssValid = false;
    disassocIsVoluntary = true;
    ic->ic_deauth_reason = IEEE80211_REASON_AUTH_LEAVE;
    if (ic->ic_state > IEEE80211_S_SCAN)
        ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
    ieee80211_disable_rsn(ic);
    ieee80211_disable_wep(ic);
    explicit_bzero(ic->ic_psk, sizeof(ic->ic_psk));
    clearWCLPMKSA();
    ic->ic_wcl_enterprise = false;
    ic->ic_flags &= ~(IEEE80211_F_PSK | IEEE80211_F_DESBSSID);
    ic->ic_des_esslen = 0;
    bzero(ic->ic_des_essid, sizeof(ic->ic_des_essid));
    return kIOReturnSuccess;
}

IOReturn AirportItlwmSkywalkInterface::abortWCLJoin()
{
    leaveWCLNetwork();
    instance->postMessage(this, AirportWCL::JoinAbortComplete, nullptr, 0, true);
    return kIOReturnSuccess;
}

void AirportItlwmSkywalkInterface::handleWCLLink(bool up)
{
    if (wclStopping)
        return;
    // The radio link is up, which only means that EAPOL frames can flow. It
    // does not mean that the 802.1X port is authorized: that happens when
    // the four way handshake finishes. The two states are kept apart, because
    // the join must not be finished before the keys are in place.
    wclLinkReady = up;
    if (!up && wclLinkIndicated) {
        AirportWCL::LinkStatusEvent event = {};
        memcpy(event.bssid, joinBSSID, sizeof(event.bssid));
        event.reason = 255;
        wclLinkIndicated = false;
        wclLinkUpReported = false;
        wclBssValid = false;
        instance->postMessage(this, AirportWCL::LinkStatus, &event, sizeof(event), true);
    }
    if (up && joinPending && firstBeaconNotified) {
        // A personal network is authorized by the association itself. An
        // enterprise network is only authorized when the 802.1X port becomes
        // valid, which is reported separately; finishing the join here would
        // cancel the join timer before the handshake ran.
        ieee80211com *ic = fHalService ? fHalService->get80211Controller() : nullptr;
        if (!ic || !ic->ic_wcl_enterprise || portAuthorized)
            finishWCLJoin(0, 0, kIOReturnSuccess);
    }
    updateWCLLinkQuality(true);
}

void AirportItlwmSkywalkInterface::handleWCLEvent(int code, void *data)
{
    ieee80211com *ic = fHalService ? fHalService->get80211Controller() : NULL;
    if (wclStopping)
        return;
    // Authentication and association traffic counts as activity for the sleep
    // on inactivity timer. Beacons do not: they arrive several times per
    // second even on a completely idle link.
    if (code != IEEE80211_EVT_BEACON && code != IEEE80211_EVT_SCAN_DONE)
        noteWCLSOIActivity();
    // The enterprise handshake finishes when the 802.1X port becomes
    // authorized. The link state was already raised when the join started, so
    // the regular link-up callback never fires for enterprise joins and the
    // join would only end through the 30 second timeout, tearing down a
    // working connection.
    if (joinPending && assocNotified && ic && ic->ic_bss &&
        ic->ic_bss->ni_port_valid) {
        // The four way handshake finished, so the 802.1X port is authorized.
        // This is the only place where that is announced: the link up
        // callback only means that EAPOL frames can flow.
        portAuthorized = true;
        // The link status that was held back until the four way handshake
        // was done is reported by the notification below. The first beacon
        // is reported here, because the beacon branch skipped it while the
        // notification was held back.
        if (wclFirstBeaconHeld) {
            wclFirstBeaconHeld = false;
            firstBeaconNotified = true;
            AirportWCL::FirstBeaconEvent first = {};
            memcpy(first.peer.bssid, joinBSSID, 6);
            instance->postMessage(this, AirportWCL::FirstBeacon, &first, sizeof(first), true);
        }
        // The link was already reported as up when the first beacon arrived,
        // long before the 802.1X port was authorized. The system needs to see
        // the transition to an authorized link, otherwise it tears the session
        // down after its own 802.1X timeout. The notification is sent even
        // when the link is already up, it carries the authorization.
        AirportWCL::LinkStatusEvent link = {};
        memcpy(link.bssid, joinBSSID, sizeof(link.bssid));
        link.up = 1;
        link.reason = 255;
        wclLinkIndicated = true;
        instance->postMessage(this, AirportWCL::LinkStatus, &link, sizeof(link), true);
        // The driver ran the 4-way handshake with the PMK that the user space
        // supplicant delivered. Report it, otherwise eapolclient waits for the
        // handshake notification, times out and tears the session down with an
        // EAPOL Logoff, which makes the AP deauthenticate us.
        //
        // Only the WCL event channel may be used here: this runs in the
        // receive interrupt context, and the synchronous interface entry
        // points of IO80211Family assert when they are called from it
        // ("trying to send on thread panic" @IO80211Glue.cpp:417).
        instance->postMessage(this, APPLE80211_M_RSN_HANDSHAKE_DONE, nullptr, 0, false);
        // The legacy event does not reach the framework's 802.1X state machine.
        // The typed supplicant event that it waits for is sent from the driver
        // work queue, together with the other legacy notifications.
        deferWCLAction(WCLDeferredHandshakeNotify);
        finishWCLJoin(0, 0, kIOReturnSuccess);
    }
    // Firmware initialization runs on systq, outside the native PostOffice gate.
    // Serialize only lifecycle notifications; never run synchronous WCL ioctls here.
    if ((code == IEEE80211_EVT_DRIVER_RESET_BEGIN || code == IEEE80211_EVT_DRIVER_READY ||
         code == IEEE80211_EVT_DRIVER_STOPPED) && !instance->getWorkQueue()->inGate()) {
        _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(12),
                                reinterpret_cast<void *>(uintptr_t(code)));
        return;
    }
    switch (code) {
        case IEEE80211_EVT_COUNTRY_CODE_UPDATE:
            deferWCLCountryUpdate();
            break;
        case IEEE80211_EVT_DRIVER_RESET_BEGIN: {
            portAuthorized = false;
            wclLinkReady = false;
            updateWCLLinkQuality(true);
            if (ic->ic_wcl_mac_reconfig) {
                break;
            }
            uint32_t reset = 0; // Native resetInterfacesBegin.
            instance->postMessage(this, AirportWCL::ResetInterface, &reset, sizeof(reset), true);
            break;
        }
        case IEEE80211_EVT_DRIVER_READY:
        case IEEE80211_EVT_DRIVER_STOPPED: {
            bool ready = code == IEEE80211_EVT_DRIVER_READY;
            if (!ready) {
                finishWCLScan(ic->ic_wcl_mac_reconfig ? kIOReturnAborted : kIOReturnOffline);
                finishWCLJoin(1, 0, kIOReturnOffline);
                portAuthorized = false;
                wclLinkReady = false;
                updateWCLLinkQuality(true);
                ic->ic_wcl_join_requested = false;
                clearWCLPMKSA();
                ic->ic_wcl_enterprise = false;
                explicit_bzero(ic->ic_psk, sizeof(ic->ic_psk));
            }
            // An address application is requested by WCL itself, not a new
            // adapter reset. Replaying firmware-load events here rotates the
            // scan MAC again and recursively restarts the card.
            if (AirportWCL::consumeMacReconfiguration(ic->ic_wcl_mac_reconfig, ready)) {
                if (ready)
                    resumeWCLJoin();
                break;
            }
            if (ready) {
                uint32_t reset = 1; // Native resetInterfacesComplete, only after HAL success.
                instance->postMessage(this, AirportWCL::ResetInterface, &reset, sizeof(reset), true);
            }
            AirportWCL::DriverAvailability event = {};
            event.version = 3;
            event.available = ready;
            event.reason = ready ? 0xe0821803 : 0xe0821804;
            instance->postMessage(this, AirportWCL::DriverAvailable, &event, sizeof(event), true);
            if (ready)
                resumeWCLJoin();
            break;
        }
        case IEEE80211_EVT_BEACON: {
            auto beacon = static_cast<ieee80211_beacon_event *>(data);
            if (!beacon)
                break;
            // Retain the actual associated beacon, including its advancing TSF.
            // WCL polls BEACON_INFO to distinguish fresh reception from a stale BSS.
            if (assocNotified && (joinPending || wclLinkReady) &&
                IEEE80211_ADDR_EQ(beacon->node->ni_bssid, joinBSSID) &&
                beacon->frame && beacon->frame_length >= 36 &&
                beacon->frame_length <= 0x800 &&
                (beacon->frame[0] & 0xfc) == IEEE80211_FC0_SUBTYPE_BEACON) {
                uint32_t length = uint32_t(beacon->frame_length);
                bzero(wclBeaconInfo, sizeof(wclBeaconInfo));
                memcpy(wclBeaconInfo, &length, sizeof(length));
                memcpy(wclBeaconInfo + 4, beacon->frame, length);
                ++wclAssociatedBeacons;
                ++wclLqmBeacons;
                // WCL has no legacy LQMData instance at createLQMData().
                // Send the snapshot consumed by WCL instead of legacy 227/229.
                if (wclLinkReady && (wclLqmBeacons % 8 == 0)) {
#if __IO80211_TARGET == __MAC_15_2
                    apple80211_rssi_data rssi = {};
                    if (getRSSI(&rssi) == kIOReturnSuccess) {
                        AirportWCL::SequoiaLqmUpdate update = {};
                        update.rssiValid = 1;
                        update.rssi = rssi.aggregate_rssi;
                        instance->postMessage(this, AirportWCL::LqmUpdate,
                            &update, sizeof(update), true);
                    }
#else
                    AirportWCL::LqmBeaconUpdate update;
                    if (AirportWCL::makeBeaconUpdate(
                            wclLqmBeacons - wclLqmReportedBeacons, update)) {
                        instance->postMessage(this, AirportWCL::LqmUpdate,
                            &update, sizeof(update), true);
                        wclLqmReportedBeacons = wclLqmBeacons;
                    }
#endif
                    updateWCLLinkQuality();
                }
            }
            if (scanPending || joinPending || ic->ic_state == IEEE80211_S_SCAN ||
                (wclLinkReady && IEEE80211_ADDR_EQ(beacon->node->ni_bssid, joinBSSID)))
                publishWCLBeacon(beacon->node, beacon->ies, beacon->length);
            if (joinPending && assocNotified && !firstBeaconNotified &&
                IEEE80211_ADDR_EQ(beacon->node->ni_bssid, joinBSSID)) {
                AirportWCL::FirstBeaconEvent event = {};
                memcpy(event.peer.bssid, joinBSSID, 6);
                if (!wclBssValid)
                    break;
                // A personal network must not report the link up before the
                // four way handshake is done. IO80211 sometimes leaves the
                // network right away when it sees the link up, the port is
                // authorized a few milliseconds too late and the join of
                // airportd then runs into its ten second timeout. The link
                // status is therefore held back until the port is
                // authorized.
                //
                // An enterprise network needs the opposite: the link status
                // is what makes the framework run setCurrentBSS, and that is
                // the only way the BSSID and the SSID reach the 802.1X
                // supplicant. It was already sent when the association
                // completed, so nothing is held back there.
                if (!portAuthorized && !ic->ic_wcl_enterprise) {
                    wclFirstBeaconHeld = true;
                    break;
                }
                firstBeaconNotified = true;
                // The link status is reported when the association completes,
                // so that the framework learns about the link even when no
                // beacon arrives after the association is through. Only send it
                // here when it has not been sent yet.
                if (!wclLinkUpReported) {
                    AirportWCL::LinkStatusEvent link = {};
                    memcpy(link.bssid, joinBSSID, sizeof(link.bssid));
                    link.up = 1;
                    link.reason = 255;
                    wclLinkUpReported = true;
                    wclLinkIndicated = true;
                    instance->postMessage(this, AirportWCL::LinkStatus, &link, sizeof(link), true);
                }
                instance->postMessage(this, AirportWCL::FirstBeacon, &event, sizeof(event), true);
                if (portAuthorized)
                    finishWCLJoin(0, 0, kIOReturnSuccess);
            }
            break;
        }
        case IEEE80211_EVT_SCAN_DONE:
            if (scanPending) {
                if (scanSkipCompletion)
                    scanSkipCompletion = false;
                else {
                    scanCompleted = true;
                    scanSource->setTimeoutMS(0);
                }
            }
            break;
        case IEEE80211_EVT_STA_ASSOC_DONE:
            if (joinPending && !assocNotified) {
                AirportWCL::AssociationEvent event = {};
                event.status = ic->ic_assoc_status;
                if (!event.status && ic->ic_bss) {
                    bool joinBSSIDKnown = false;
                    for (unsigned i = 0; i < 6; ++i)
                        joinBSSIDKnown |= joinBSSID[i] != 0;
                    if (!joinBSSIDKnown)
                        memcpy(joinBSSID, ic->ic_bss->ni_bssid, sizeof(joinBSSID));
                    if (wclHasHiddenSsid)
                        memcpy(wclHiddenBssid, ic->ic_bss->ni_bssid, sizeof(wclHiddenBssid));
                    setCurrentApAddress(reinterpret_cast<ether_addr *>(ic->ic_bss->ni_bssid));
                    if (ic->ic_wcl_enterprise) {
                        if (ic->ic_rsnprotos == 0)
                            ic->ic_rsnprotos = IEEE80211_PROTO_RSN;
                        if (ic->ic_rsnakms == 0)
                            ic->ic_rsnakms = IEEE80211_AKM_8021X;
                        if (ic->ic_rsnciphers == 0)
                            ic->ic_rsnciphers = IEEE80211_CIPHER_CCMP;
                        if (ic->ic_rsngroupcipher == 0)
                            ic->ic_rsngroupcipher = IEEE80211_CIPHER_CCMP;
                        ic->ic_flags |= IEEE80211_F_RSNON;
                        ic->ic_bss->ni_rsnprotos = ic->ic_rsnprotos;
                        ic->ic_bss->ni_rsnakms = ic->ic_rsnakms;
                        ic->ic_bss->ni_rsncipher =
                            (ic->ic_rsnciphers & IEEE80211_CIPHER_CCMP)
                            ? IEEE80211_CIPHER_CCMP : IEEE80211_CIPHER_TKIP;
                        ic->ic_bss->ni_rsngroupcipher = ic->ic_rsngroupcipher;
                        // The link must not be raised here: this runs in the
                        // receive interrupt context and IO80211Family asserts
                        // that the interface is only touched from its own work
                        // queue thread ("trying to send on thread panic"
                        // @IO80211Glue.cpp:417). The asynchronous WCL link
                        // status message is the channel the framework listens
                        // on.
                        //
                        // When the join request named a BSSID the link was
                        // already raised there, in the context of the
                        // framework's ioctl, so nothing has to be sent.
                        {
                            // The link status above is what makes the framework
                            // run setCurrentBSS, and that is the only way the
                            // BSSID and the SSID reach the 802.1X supplicant.
                            // Without it the supplicant does not know the
                            // network it has to authenticate on, refuses to
                            // send anything and the join ends in "join
                            // failure".
                            //
                            // It used to be sent from the beacon branch
                            // instead, where it only happened when a beacon
                            // arrived after the association was through. A
                            // beacon that arrives before the association is
                            // complete therefore lost the notification for
                            // good, and the join never recovered on its own.
                            AirportWCL::LinkStatusEvent link = {};
                            memcpy(link.bssid, joinBSSID, sizeof(link.bssid));
                            link.up = 1;
                            link.reason = 255;
                            wclLinkUpReported = true;
                            wclLinkIndicated = true;
                            instance->postMessage(this, AirportWCL::LinkStatus,
                                                  &link, sizeof(link), true);
                        }
                    }
                }
                memcpy(event.bssid, joinBSSID, 6);
                event.driverStatus = event.status ? kIOReturnError : kIOReturnSuccess;
                assocNotified = !event.status;
                instance->postMessage(this, AirportWCL::AssocComplete, &event, sizeof(event), true);
                // The kernel side 802.1X state machine learns about the
                // finished association from the legacy event and only then
                // starts the user space supplicant. Without it the supplicant
                // is not woken up, the framework runs into its ten second
                // join timeout and the session is left with "join failure".
                instance->postMessage(this, APPLE80211_M_ASSOC_DONE, nullptr, 0, false);
                // The supplicant also needs the network it is authenticating
                // on. It asks the driver for the BSSID and refuses to send
                // anything while the answer is unknown, so the change of the
                // BSSID is announced together with the association.
                instance->postMessage(this, APPLE80211_M_BSSID_CHANGED, nullptr, 0, false);
                instance->postMessage(this, APPLE80211_M_SSID_CHANGED, nullptr, 0, false);
                instance->postMessage(this, APPLE80211_M_LINK_CHANGED, nullptr, 0, false);
                if (event.status)
                    finishWCLJoin(event.status, 0, kIOReturnError);
            }
            break;
        case IEEE80211_EVT_STA_DEAUTH: {
            portAuthorized = false;
            wclLinkReady = false;
            // This runs in the receive interrupt context: the link quality
            // update reaches IO80211Family and would have to block on the
            // command gate. Hand it to the driver work queue.
            deferWCLAction(WCLDeferredLinkQuality, true);
            AirportWCL::DeauthEvent event = {};
            event.reason = ic->ic_deauth_reason;
            if (ic->ic_bss)
                memcpy(event.bssid, ic->ic_bss->ni_bssid, 6);
            instance->postMessage(this, AirportWCL::Deauth, &event, sizeof(event), true);
            finishWCLJoin(1, event.reason, kIOReturnOffline);
            break;
        }
        default: break;
    }
}

IOReturn AirportItlwmSkywalkInterface::getWCL_VALID_CHANNEL_COUNT(unsigned long *count)
{
    if (!count)
        return kIOReturnBadArgument;
    *count = 0;
    ieee80211com *ic = fHalService->get80211Controller();
    for (unsigned i = 1; i < IEEE80211_CHAN_MAX; ++i)
        if (isset(ic->ic_chan_active, i))
            ++*count;
    return kIOReturnSuccess;
}

IOReturn AirportItlwmSkywalkInterface::setWCL_LINK_STATE_UPDATE(apple80211_wcl_update_link_state *data)
{
    if (!data)
        return kIOReturnBadArgument;
#if __IO80211_TARGET == __MAC_15_2
    const auto bytes = reinterpret_cast<const uint8_t *>(data);
    if (bytes[6]) {
        if (bytes[8])
            setCurrentApAddress(reinterpret_cast<ether_addr *>(data));
        if (bytes[7])
            wclSetLinkState(this, kIO80211NetworkLinkUp, 0, false, 0);
    } else {
        uint32_t debounce = AirportWCL::read32(bytes + 12);
        setCurrentApAddress(nullptr);
        wclSetLinkState(this, kIO80211NetworkLinkDown, 0, debounce != 0, debounce);
    }
    IOReturn status = kIOReturnSuccess;
#else
    IOReturn status = IO80211InfraInterface::setWCL_LINK_STATE_UPDATE(data);
#endif
    if (status == kIOReturnSuccess)
        updateWCLLinkQuality(true);
    return status;
}

IOReturn AirportItlwmSkywalkInterface::getWCL_BSS_INFO(apple80211_beacon_msg *data)
{
    if (!data)
        return kIOReturnBadArgument;
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(5), data);
}

IOReturn AirportItlwmSkywalkInterface::getWCL_EXTENDED_BSS_INFO(apple80211_extended_bss_info *data)
{
    if (!data)
        return kIOReturnBadArgument;
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(6), data);
}

IOReturn AirportItlwmSkywalkInterface::copyWCLExtendedBss(void *data)
{
    ieee80211com *ic = fHalService->get80211Controller();
    // The framework asks for the extended BSS information as soon as the link
    // comes up, which is right after the association. Waiting for the running
    // state would be too late: that is only reached once the 802.1X port is
    // authorized, and the framework gives up on the network when the query
    // fails ("Update Bss fail"), which ends the join in "join failure".
    if (!data || !ic->ic_bss || ic->ic_state < IEEE80211_S_ASSOC) {
        return kIOReturnNotReady;
    }
    static_assert(sizeof(apple80211_rate_set_data) == 0xbc, "Extended BSS rates size");
    static_assert(sizeof(apple80211_mcs_index_set_data) == 0x10, "Extended BSS MCS size");
    auto bytes = static_cast<uint8_t *>(data);
#if __IO80211_TARGET == __MAC_15_2
    bzero(bytes, 0x1e0);
#else
    bzero(bytes, 0x214);
#endif
    IOReturn status = getRATE_SET(reinterpret_cast<apple80211_rate_set_data *>(bytes));
    if (status) {
        return status;
    }
    status = getMCS_INDEX_SET(reinterpret_cast<apple80211_mcs_index_set_data *>(bytes + 0xbc));
    if (status) {
        return status;
    }
    apple80211_vht_mcs_index_set_data vht = {};
    vht.version = APPLE80211_VERSION;
    vht.mcs_map = 0xffff;
    if (ic->ic_bss->ni_flags & IEEE80211_NODE_VHT)
        vht.mcs_map = ic->ic_bss->ni_vht_mcsinfo.tx_mcs_map;
    memcpy(bytes + 0xcc, &vht, sizeof(vht));
    // Native updateMCSSet uses version + a 16-bit HE MCS/NSS map (8 bytes).
    uint32_t heVersion = APPLE80211_VERSION;
    uint16_t heMap = 0xffff;
    if (ic->ic_bss->ni_flags & IEEE80211_NODE_HE) {
        bool wide = ic->ic_bss->ni_chw == IEEE80211_CHAN_WIDTH_160;
        heMap = htole16(ieee80211_he_mcs_intersection(
            le16toh(wide ? ic->ic_he_mcs_nss_supp.rx_mcs_160 : ic->ic_he_mcs_nss_supp.rx_mcs_80),
            le16toh(wide ? ic->ic_bss->ni_he_mcs_nss_supp.tx_mcs_160 : ic->ic_bss->ni_he_mcs_nss_supp.tx_mcs_80)));
    }
    memcpy(bytes + 0xd4, &heVersion, sizeof(heVersion));
    memcpy(bytes + 0xd8, &heMap, sizeof(heMap));
    if (ic->ic_flags & IEEE80211_F_RSNON)
#if __IO80211_TARGET == __MAC_15_2
        ieee80211_add_rsn(bytes + 0xdd, ic, ic->ic_bss);
#else
        ieee80211_add_rsn(bytes + 0x113, ic, ic->ic_bss);
#endif
    return kIOReturnSuccess;
}

IOReturn AirportItlwmSkywalkInterface::setWCL_UPDATE_FAST_LANE(apple80211_fastlane *data)
{
    if (!data)
        return kIOReturnBadArgument;
    /*
     * Cisco/Apple Fast Lane: macOS asks the driver to enable it right after
     * the association completes when the AP advertises it (e.g. Aironet 3800).
     * This adapter has no Fast Lane QoS marking, but returning an error here
     * makes WCL treat it as "Update Bss fail" and tear the link down ~0.4 s
     * after LINK_STATE_IS_UP. Accept the request and ignore it.
     */
    return kIOReturnSuccess;
}

IOReturn AirportItlwmSkywalkInterface::getWCL_LOW_LATENCY_INFO(apple80211_low_latency_info *data)
{
    if (!data)
        return kIOReturnBadArgument;
    // No low-latency or P2P session is implemented by this adapter.
    bzero(data, 4);
    return kIOReturnSuccess;
}

IOReturn AirportItlwmSkywalkInterface::getWCL_CHANNELS_INFO(apple80211ChannelInfo *data)
{
    if (!data)
        return kIOReturnBadArgument;
    auto info = reinterpret_cast<AirportWCL::ChannelInfo *>(data);
    bzero(info, sizeof(*info));
    ieee80211com *ic = fHalService->get80211Controller();
    for (unsigned i = 1; i < IEEE80211_CHAN_MAX && info->count < 400; ++i) {
        if (!isset(ic->ic_chan_active, i))
            continue;
        const ieee80211_channel *channel = &ic->ic_channels[i];
        auto &entry = info->channels[info->count++];
        entry.channel = i;
        entry.channelSpec = uint16_t(i | (IEEE80211_IS_CHAN_5GHZ(channel) ? 0xd000 : 0x1000));
        if (channel->ic_flags & IEEE80211_CHAN_DFS)
            entry.flags |= 2;
        if (channel->ic_flags & IEEE80211_CHAN_PASSIVE)
            entry.flags |= 4;
    }
    apple80211_country_code_data country = {};
    if (instance->getCOUNTRY_CODE(static_cast<OSObject *>(this), &country) == kIOReturnSuccess)
        memcpy(info->country, country.cc, 3);
    return info->count ? kIOReturnSuccess : kIOReturnNotReady;
}

IOReturn AirportItlwmSkywalkInterface::getCHANNELS_INFO(apple80211_channels_info *data)
{
    if (!data)
        return kIOReturnBadArgument;
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(13), data);
}

IOReturn AirportItlwmSkywalkInterface::copyWCLChannels(void *data)
{
    auto info = static_cast<AirportWCL::NativeChannelInfo *>(data);
    bzero(info, sizeof(*info));
    ieee80211com *ic = fHalService->get80211Controller();
    for (unsigned i = 1; i <= IEEE80211_CHAN_MAX; ++i) {
        const ieee80211_channel *channel = &ic->ic_channels[i];
        if (!isset(ic->ic_chan_active, i) || !channel->ic_freq)
            continue;
        AirportWCL::appendChannel(*info, i, IEEE80211_IS_CHAN_5GHZ(channel),
            channel->ic_flags & IEEE80211_CHAN_DFS,
            channel->ic_flags & IEEE80211_CHAN_PASSIVE,
            IEEE80211_IS_CHAN_HT40(channel) || IEEE80211_IS_CHAN_VHT40(channel),
            IEEE80211_IS_CHAN_VHT80(channel));
    }
    apple80211_country_code_data country = {};
    if (instance->getCOUNTRY_CODE(static_cast<OSObject *>(this), &country) == kIOReturnSuccess)
        memcpy(info->country, country.cc, sizeof(country.cc));
    return info->count ? kIOReturnSuccess : kIOReturnNotReady;
}

IOReturn AirportItlwmSkywalkInterface::getWCL_TRAFFIC_COUNTERS(apple80211_wcl_traffic_counters *data)
{
    if (!data)
        return kIOReturnBadArgument;
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(14), data);
}

IOReturn AirportItlwmSkywalkInterface::copyWCLTrafficCounters(void *data)
{
    ieee80211com *ic = fHalService->get80211Controller();
    uint64_t timestamp;
    absolutetime_to_nanoseconds(mach_absolute_time(), &timestamp);
    const auto counters = AirportWCL::makeTrafficCounters(
        ic->ic_wcl_tx_packets, ic->ic_wcl_rx_packets, timestamp);
    memcpy(data, &counters, sizeof(counters));
    return kIOReturnSuccess;
}

IOReturn AirportItlwmSkywalkInterface::setWCL_SET_SCAN_HOME_AWAY_TIME(scanHomeAndAwayTime *data)
{
    if (!data)
        return kIOReturnBadArgument;
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(15), data);
}
#endif

#ifdef AIRPORT_WCL
IOReturn AirportItlwmSkywalkInterface::getBEACON_INFO(apple80211_beacon_info_t *data)
{
    if (!canUseCommandGate() || !data)
        return kIOReturnNotReady;
    if (!canUseCommandGate())
        return kIOReturnNotReady;
    return _fCommandGate->runAction(wclCommand, this, reinterpret_cast<void *>(7), data);
}
#endif
