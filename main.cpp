/* 
 * File:   main.cpp
 * Author: kamyshev.a
 *
 * Created on 29 August 2018, 15:50
 */

#include <cstdlib>
#include <iostream>
#include "egts_packet.h"

using namespace std;
using namespace EGTS;

static void OnError(EGTS::error_t* error) {
  std::cout << "Error, sender: " << error->sender.c_str();
  std::cout << "; message: " << error->message.c_str() << ";\r\n";
}

int main(int argc, char** argv) {
  EGTS::SetErrorCallback(OnError);
  TPacket pack;
  pack.head.pid = 10;
  try {
    TRecord *ath = pack.recs.New();
    ath->rst = EGTS_AUTH_SERVICE;
    ath->head.rn = 0;
    TRecResp *rsp = new TRecResp;
    ath->sbrs.Add(rsp);

    TVehicleData *veh = new TVehicleData;
    memcpy(veh->body.vin, "test VIN\0", 10);
    ath->sbrs.Add(veh);

    TTermIdent *tid = new TTermIdent;
    tid->head.flags.imeie = 1;
    memcpy(tid->imei, "test IMEI\0", 10);
    ath->sbrs.Add(tid);

    TDispIdent *did = new TDispIdent;
    did->dscr = "test description";
    ath->sbrs.Add(did);

    TAuthParams *apm = new TAuthParams;
    ath->sbrs.Add(apm);

    TAuthInfo *inf = new TAuthInfo;
    inf->user_name = "test name";
    inf->user_pass = "test password";
    ath->sbrs.Add(inf);

    TServiceInfo *srv = new TServiceInfo;
    srv->body.st = EGTS_EUROPROTOCOL_SERVICE;
    srv->body.sst = EGTS_SST_IN_SERVICE;
    ath->sbrs.Add(srv);

    ath->sbrs.Add(new TResultCode);

    TTermIdent2 *tid2 = new TTermIdent2;
    memcpy(tid2->iccid, "test ICCID\0", 11);
    tid2->flags.icce = 1;
    ath->sbrs.Add(tid2);

    TRecord *tds = pack.recs.New();
    tds->rst = EGTS_TELEDATA_SERVICE;
    tds->head.rn = 1;

    TPlusData *pls = new TPlusData;
    pls->protobuf.data.reset(new char[16]);
    pls->protobuf.size = 16;
    tds->sbrs.Add(pls);

    TPosData *pos = new TPosData;
    pos->SetLatitude(55.5088);
    pos->SetLongitude(37.3345);
    pos->SetSpeed(100);
    pos->SetCourse(180);
    pos->SetTime(time(0));
    tds->sbrs.Add(pos);

    tds->sbrs.Add(new TExtPosData);

    TAdSensors *ads = new TAdSensors;
    ads->SetAin(2, 500);
    ads->SetDin(3, 1);
    tds->sbrs.Add(ads);

    TCounters *cnt = new TCounters;
    cnt->SetCounter(4, 3000);
    tds->sbrs.Add(cnt);

    tds->sbrs.Add(new TStateData);

    TLiquidLevel *llv = new TLiquidLevel;
    llv->llcd.data.reset(new char[16]);
    llv->llcd.size = 16;
    tds->sbrs.Add(llv);

    TPassengers *psg = new TPassengers;
    TPassengers::door_num_t *door = psg->doors.New();
    door->num = 3;
    door->door.psgs_in = 10;
    door->door.psgs_out = 5;
    tds->sbrs.Add(psg);

    TRecord *ecs = pack.recs.New();
    ecs->head.rn = 2;
    ecs->rst = EGTS_ECALL_SERVICE;

    TECallRawMSD *raw = new TECallRawMSD;
    raw->msd.data.reset(new char[16]);
    raw->msd.size = 16;
    ecs->sbrs.Add(raw);

    TECallMSD *msd = new TECallMSD;
    msd->bf.lond2 = 1;
    msd->additional.data.reset(new char[16]);
    msd->additional.size = 16;
    ecs->sbrs.Add(msd);

    TRecord *ins = pack.recs.New();
    ins->head.rn = 3;
    ins->rst = EGTS_INSURANCE_SERVICE;
    ins->sbrs.Add(new TInsAccel);

    TRecord *eps = pack.recs.New();
    eps->rst = EGTS_EUROPROTOCOL_SERVICE;
    eps->head.rn = 4;

    eps->sbrs.Add(new TEPMainData);

    TEPTrackData *track = new TEPTrackData;
    eps->sbrs.Add(track);

    std::unique_ptr<EGTS::TEPCompData> cmp(new TEPCompData);
    if (cmp->Compress(track)) eps->sbrs.Add(cmp.release());

    eps->sbrs.Add(new TEPAccelData3);

    TEPSignature *sign = new TEPSignature;
    TEPSignData *sd = sign->sd_list.New();
    sd->sd.reset(new char[72]);
    memcpy(sd->sd.get(), "test signature\0", 15);
    sd->SetSignLength(72);
    eps->sbrs.Add(sign);

  } catch (std::exception &e) {
    std::cout << e.what() << "\r\n";
    return 1;
  }

  uint16_t size;
  std::unique_ptr<char> ptr(pack.GetData(&size));
  pack.Init();
  pack.SetData(ptr.get(), size);
  for (TRecord *r = pack.recs.First(); r; r = pack.recs.Next()) {
    std::cout << "record, rn = " << r->head.rn << ", rl = " << r->head.rl;
    std::cout << ", rst = " << (int16_t) r->rst << "\r\n";
    for (TSubrecord *sbr = r->sbrs.First(); sbr; sbr = r->sbrs.Next()) {
      uint16_t srl;
      try {
        sbr->GetData(&srl);
      } catch (std::exception &e) {
        std::cout << e.what() << "\r\n";
        return 1;
      }
      std::cout << "subrecord, srt = " << (int16_t) sbr->GetType() << ", srl = ";
      std::cout << srl - 3 << "\r\n";
      uint8_t type = sbr->GetType();
      if (r->rst == EGTS_AUTH_SERVICE) {
        if (type == EGTS_SR_TERM_IDENTITY) {
          TTermIdent *tid = dynamic_cast<TTermIdent*> (sbr);
          if (tid) std::cout << tid->imei << "\r\n";
        } else if (type == EGTS_SR_TERM_IDENTITY2) {
          TTermIdent2 *tid2 = dynamic_cast<TTermIdent2*> (sbr);
          if (tid2) std::cout << tid2->iccid << "\r\n";
        } else if (type == EGTS_SR_DISPATCHER_IDENTITY) {
          TDispIdent *did = dynamic_cast<TDispIdent*> (sbr);
          if (did) std::cout << did->dscr << "\r\n";
        } else if (sbr->GetType() == EGTS_SR_AUTH_INFO) {
          TAuthInfo *inf = dynamic_cast<TAuthInfo*> (sbr);
          if (inf) std::cout << inf->user_name << ";" << inf->user_pass << "\r\n";
        } else if (sbr->GetType() == EGTS_SR_SERVICE_INFO) {
          TServiceInfo *srv = dynamic_cast<TServiceInfo*> (sbr);
          if (!srv) continue;
          std::cout << "ST = " << (uint16_t) srv->body.st;
          std::cout << ", SST = " << (uint16_t) srv->body.sst << "\r\n";
        } else if (type == EGTS_SR_VEHICLE_DATA) {
          TVehicleData *veh = dynamic_cast<TVehicleData*> (sbr);
          if (veh) std::cout << veh->body.vin << "\r\n";
        }
      } else if (r->rst == EGTS_TELEDATA_SERVICE) {
        if (type == EGTS_SR_POS_DATA) {
          TPosData *pos = dynamic_cast<TPosData*> (sbr);
          if (!pos) continue;
          std::cout << "lat = " << pos->GetLatitude();
          std::cout << ", lon = " << pos->GetLongitude() << ", speed = ";
          std::cout << pos->GetSpeed() << ", course = " << pos->GetCourse();
          std::cout << ", time = " << pos->GetTime() << "\r\n";
        } else if (type == EGTS_SR_AD_SENSORS_DATA) {
          TAdSensors *ads = dynamic_cast<TAdSensors*> (sbr);
          if (!ads) continue;
          std::cout << "ain2 = " << ads->GetAin(2);
          std::cout << ", din3 = " << (uint16_t) ads->GetDin(3) << "\r\n";
        } else if (type == EGTS_SR_COUNTERS_DATA) {
          TCounters *cnt = dynamic_cast<TCounters*> (sbr);
          if (cnt)
            std::cout << "counter4 = " << cnt->GetCounter(4) << "\r\n";
        } else if (type == EGTS_SR_PASSENGERS_COUNTERS) {
          TPassengers *psg = dynamic_cast<TPassengers*> (sbr);
          if (!psg) continue;
          TPassengers::door_num_t *door = psg->doors.First();
          for (; door; door = psg->doors.Next()) {
            std::cout << "door" << (uint16_t) door->num << ", in = " <<
                (uint16_t) door->door.psgs_in << ", out = " <<
                (uint16_t) door->door.psgs_out << "\r\n";
          }
        }
      } else if (r->rst == EGTS_EUROPROTOCOL_SERVICE) {
        if (type == EGTS_SR_EP_COMP_DATA) {
          TEPMainData *md = dynamic_cast<TEPMainData*> (sbr);
          if (md) std::cout << "bn = " << md->GetBlockNumber();
        } else if (type == EGTS_SR_EP_COMP_DATA) {
          TEPCompData *cmp = dynamic_cast<TEPCompData*> (sbr);
          if (!cmp) continue;
          std::unique_ptr<TSubrecord> unpacked = cmp->Extract();
          if (!unpacked) continue;
          std::cout << "unpacked subrecord, srt = " << (int16_t) unpacked->GetType() << "\r\n";
        } else if (type == EGTS_SR_EP_SIGNATURE) {
          TEPSignature *sign = dynamic_cast<TEPSignature*> (sbr);
          if (!sign) continue;
          TEPSignData *ds = sign->sd_list.First();
          if (!ds) continue;
          std::cout << "signature length = " << ds->GetSignLength() << ", " << ds->sd.get();
        }
      }
    }
  }
  return 0;
}


