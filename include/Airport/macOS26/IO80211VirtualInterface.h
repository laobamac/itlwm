//
//  IO80211VirtualInterface.h
//  AirportItlwm
//
//  macOS 26 (Tahoe) ABI, generated from the kernel collection symbols.
//  from the IO80211Family vtable in the Tahoe kernel collection.
//

#ifndef IO80211VirtualInterface_h
#define IO80211VirtualInterface_h

#include <IOKit/IOService.h>
#include <Airport/apple80211_var.h>
#include <Airport/apple80211_ioctl.h>
#include "IO80211SkywalkInterface.h"

struct __ifnet;
struct apple80211_ManagementInformationBasedot11_counters;
struct apple80211_chip_counters_rx;
struct apple80211_chip_counters_tx;
struct apple80211_chip_error_counters_tx;
struct apple80211_debug_command;
struct apple80211_frame_counters;
struct apple80211_infra_specific_stats;
struct apple80211_interface_availability;
struct apple80211_scan_data;
struct apple80211_stat_report;
struct ether_addr;
struct ether_header;
struct packet_info_tag;
struct peerSlotDataStats;
struct packetCounters_t;

class IO80211VirtualInterface : public IO80211SkywalkInterface {
    OSDeclareAbstractStructors(IO80211VirtualInterface)

public:
    virtual void free();  // 0x0090 override
    virtual IOReturn configureReport(IOReportChannelList*, unsigned int, void*, void*);  // 0x0428 override
    virtual IOReturn updateReport(IOReportChannelList*, unsigned int, void*, void*);  // 0x0430 override
    virtual bool start(IOService*);  // 0x05c0 override
    virtual void stop(IOService*);  // 0x05c8 override
    virtual bool terminate(unsigned int);  // 0x0600 override
    virtual bool attach(IOService*);  // 0x0670 override
    virtual void detach(IOService*);  // 0x0678 override
    virtual IOReturn newUserClient(task*, void*, unsigned int, OSDictionary*, IOUserClient**);  // 0x0770 override
    virtual const char * stringFromReturn(int);  // 0x0780 override
    virtual IOReturn enable(unsigned int);  // 0x0850 override
    virtual IOReturn prepareBSDInterface(__ifnet*, unsigned int);  // 0x08e8 override
    virtual IOReturn processBSDCommand(__ifnet*, unsigned int, void*);  // 0x0910 override
    virtual SInt32 setInterfaceEnable(bool);  // 0x0930 override
    virtual UInt32 getFeatureFlags();  // 0x09b8 override
    virtual bool bpfTap(unsigned int, unsigned int);  // 0x09e0 override
    virtual bool setMulticastAddresses(ether_addr const*, unsigned int);  // 0x0a88 override
    virtual void * createPeerManager();  // 0x0b08 override
    virtual void postMessage(unsigned int, void*, unsigned long, bool);  // 0x0b18 override
    virtual void logTxPacket(IO80211NetworkPacket*, PacketSkywalkScratch*, apple80211_wme_ac, bool);  // 0x0b38 override
    virtual void logTxCompletionPacket(IO80211NetworkPacket*, PacketSkywalkScratch*, unsigned char*, apple80211_wme_ac, int, unsigned int, bool, bool);  // 0x0b40 override
    virtual IOReturn inputPacket(IO80211NetworkPacket*, packet_info_tag*, ether_header*, bool*, bool);  // 0x0b50 override
    virtual void logSkywalkTxReqPacket(IO80211NetworkPacket*, PacketSkywalkScratch*, unsigned char*, apple80211_wme_ac, bool);  // 0x0b60 override
    virtual SInt64 pendingPackets(unsigned char);  // 0x0b68 override
    virtual SInt64 packetSpace(unsigned char);  // 0x0b70 override
    virtual IO80211LinkState linkState();  // 0x0b88 override
    virtual void setScanningState(unsigned int, bool, apple80211_scan_data*, int);  // 0x0b90 override
    virtual void updateLinkParameters(apple80211_interface_availability*);  // 0x0bb0 override
    virtual void updateInterfaceCoexRiskPct(unsigned long long);  // 0x0bb8 override
    virtual void setInterfaceNF(apple80211_channel, long long);  // 0x0be8 override
    virtual void setDebugFlags(unsigned long long, unsigned int);  // 0x0c00 override
    virtual SInt64 debugFlags();  // 0x0c08 override
    virtual void setInterfaceChipCounters(apple80211_stat_report*, apple80211_chip_counters_tx*, apple80211_chip_error_counters_tx*, apple80211_chip_counters_rx*);  // 0x0c10 override
    virtual void setInterfaceMIBdot11(apple80211_stat_report*, apple80211_ManagementInformationBasedot11_counters*);  // 0x0c18 override
    virtual void setFrameStats(apple80211_stat_report*, apple80211_frame_counters*);  // 0x0c20 override
    virtual void setInfraSpecificFrameStats(apple80211_stat_report*, apple80211_infra_specific_stats*);  // 0x0c28 override
    virtual SInt64 getWmeTxCounters(unsigned long long*);  // 0x0c38 override
    virtual void setWoWEnabled(bool);  // 0x0c48 override
    virtual bool wowEnabled();  // 0x0c50 override
    virtual void printDataPath(userPrintCtx*);  // 0x0c58 override
private: virtual void _abi_0c68() __asm__("__ZN23IO80211VirtualInterface21findOrCreateFlowQueueE20IO80211FlowQueueHash"); public:
private: virtual void _abi_0c70() __asm__("__ZN23IO80211VirtualInterface30findOrCreateFlowQueueWithCacheE20IO80211FlowQueueHashPb"); public:
private: virtual void _abi_0c78() __asm__("__ZN23IO80211VirtualInterface21findExistingFlowQueueE20IO80211FlowQueueHash"); public:
private: virtual void _abi_0c80() __asm__("__ZN23IO80211VirtualInterface17removePacketQueueEPK20IO80211FlowQueueHash"); public:
    virtual void flushPacketQueues();  // 0x0c88 override
    virtual void cachePeer(ether_addr*, unsigned int*);  // 0x0c90 override
    virtual bool shouldLog(unsigned long long);  // 0x0c98 override
    virtual void vlogDebug(unsigned long long, char const*, va_list);  // 0x0ca0 override
    virtual void vlogDebugBPF(unsigned long long, char const*, va_list);  // 0x0ca8 override
    virtual void * getP2PSkywalkPeerMgr();  // 0x0cc0 override
private: virtual void _abi_0cd0() __asm__("__ZN23IO80211VirtualInterface8findPeerER10ether_addr"); public:
    virtual int getEventPipeSize();  // 0x0db8 override
    virtual UInt64 createEventPipe(IO80211APIUserClient*);  // 0x0dc0 override
    virtual bool init(IO80211Controller*, ether_addr*, unsigned int, char const*);
    virtual IOReturn setLinkState(IO80211LinkState, unsigned int);
    virtual IOReturn forwardPacket(IO80211NetworkPacket*);
    virtual IOReturn handleIoctl(__ifnet*, unsigned long, void*);
    virtual IOReturn addPeerToCache(unsigned char*);
    virtual IOReturn deletePeerFromCache(unsigned char*);
    virtual IOReturn handleDebugCmd(apple80211_debug_command*);
    virtual IOReturn getPeerSlotDataStats(ether_addr*, bool, peerSlotDataStats*);
    virtual IOReturn getPeerDataStats(ether_addr*, bool, packetCounters_t*);
    virtual IOReturn clearPeerDataStats(ether_addr*, bool);
    virtual IOReturn setAwdlCurrentChannelSequenceIndex(unsigned int);
    virtual bool isOutputFlowControlled();
    virtual IOReturn setOutputFlowControlled();
    virtual IOReturn clearOutputFlowControlled();
    virtual IOReturn flushFlowQueues(ether_addr*);
    virtual IOReturn configureAQMOutput();
    virtual IOReturn attachToBpf();
    virtual IOReturn configureIfnet();
    virtual void reset();
    virtual IOReturn dupAndTransmitMcastPacket(IO80211NetworkPacket*, unsigned char*, unsigned char*);
    virtual IOReturn freeMulticastPacket(IO80211NetworkPacket*);
    virtual IOReturn triggerAWDLMulticastTx(unsigned int, unsigned char);
    virtual IOReturn p2pMulticastTx(IO80211NetworkPacket**, unsigned int);
    virtual IOReturn p2pMulticastTx(unsigned int, bool);
    virtual IOReturn signalMulticastPackets(unsigned int, unsigned int);
    virtual IOReturn sendPendingPacketsToStack(IO80211NetworkPacket**, unsigned int);
    virtual IOReturn freePendingPackets(IO80211NetworkPacket**, unsigned int);
    virtual IOReturn setMacAddress(ether_addr&) = 0; // 0x0f40
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 0);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 1);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 2);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 3);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 4);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 5);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 6);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 7);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 8);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 9);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 10);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 11);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 12);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 13);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 14);
    OSMetaClassDeclareReservedUnused(IO80211VirtualInterface, 15);

    // IO80211VirtualInterface::init stores its ivars pointer at 0x128
    // (IO80211SkywalkInterface ends there); the Apple object is 0x130 bytes.
private: uint8_t _layout[0x130 - sizeof(IO80211SkywalkInterface)];
};
static_assert(sizeof(IO80211SkywalkInterface) == 0x128, "Tahoe ABI size");
static_assert(sizeof(IO80211VirtualInterface) == 0x130, "Tahoe ABI size");

#endif /* IO80211VirtualInterface_h */
