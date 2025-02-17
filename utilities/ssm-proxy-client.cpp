#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <iostream>
#include <string>

#include "ssm.h"
#include "ssm-proxy-client.hpp"
#include "ssm-proxy-client-child.hpp"
#include "dssm-utility.hpp"

#include <chrono>
using namespace std::chrono;
#define FOR_DEBUG 0

PConnector::PConnector() :
		tbuf(nullptr), time(0.0), ipaddr("127.0.0.1") {
	initPConnector();
}

PConnector::PConnector(const char *streamName, int streamId, const char *ipAddress) :
		tbuf(nullptr), time(0.0), ipaddr(ipAddress) {
	initPConnector();
	setStream(streamName, streamId);
}

PConnector::~PConnector() {
	if (sock != -1) {
		close(sock);
	}
	if (this->tbuf) {
		free(this->tbuf);
	}
}

void PConnector::initPConnector() {
	sock = -1;
	dsock = -1;
	mDataSize = 0;
	streamId = 0;
	sid = 0;
	mData = NULL;
	mFullData = NULL;
	mProperty = NULL;
	mPropertySize = 0;
	streamName = "";
	mFullDataSize = 0;
	openMode = SSM_READ;
	timeId = -1;
	timecontrol = NULL;
	isVerbose = false;
	isBlocking = false;
	tbuf = (char*) malloc(sizeof(thrd_msg));
	thrdMsgLen = dssm::util::countThrdMsgLength();
	dssmMsgLen = dssm::util::countDssmMsgLength();
	memset(tbuf, 0, sizeof(thrd_msg));
}

int PConnector::readInt(char **p) {
	uint8_t v1 = **p;
	(*p)++;
	uint8_t v2 = **p;
	(*p)++;
	uint8_t v3 = **p;
	(*p)++;
	uint8_t v4 = **p;
	(*p)++;

	int v = (int) (v1 << 24 | v2 << 16 | v3 << 8 | v4);
	return v;
}

uint64_t PConnector::readLong(char **p) {
	uint8_t v1 = **p;
	(*p)++;
	uint8_t v2 = **p;
	(*p)++;
	uint8_t v3 = **p;
	(*p)++;
	uint8_t v4 = **p;
	(*p)++;
	uint8_t v5 = **p;
	(*p)++;
	uint8_t v6 = **p;
	(*p)++;
	uint8_t v7 = **p;
	(*p)++;
	uint8_t v8 = **p;
	(*p)++;

	uint64_t lv = (uint64_t) ((uint64_t) v1 << 56 | (uint64_t) v2 << 48
			| (uint64_t) v3 << 40 | (uint64_t) v4 << 32 | (uint64_t) v5 << 24
			| (uint64_t) v6 << 16 | (uint64_t) v7 << 8 | (uint64_t) v8);
	return lv;
}

double PConnector::readDouble(char **p) {
	char buf[8];
	for (int i = 0; i < 8; ++i, (*p)++) {
		buf[7 - i] = **p;
	}
	return *(double*) buf;
}

void PConnector::readRawData(char **p, char *d, int len) {
	for (int i = 0; i < len; ++i, (*p)++) {
		d[i] = **p;
	}
}

void PConnector::writeInt(char **p, int v) {
	**p = (v >> 24) & 0xff;
	(*p)++;
	**p = (v >> 16) & 0xff;
	(*p)++;
	**p = (v >> 8) & 0xff;
	(*p)++;
	**p = (v >> 0) & 0xff;
	(*p)++;
}

void PConnector::writeLong(char **p, uint64_t v) {
	**p = (v >> 56) & 0xff;
	(*p)++;
	**p = (v >> 48) & 0xff;
	(*p)++;
	**p = (v >> 40) & 0xff;
	(*p)++;
	**p = (v >> 32) & 0xff;
	(*p)++;
	this->writeInt(p, v);
}

void PConnector::writeDouble(char **p, double v) {
	char *dp = (char*) &v;
	for (int i = 0; i < 8; ++i, (*p)++) {
		**p = dp[7 - i] & 0xff;
	}
}

void PConnector::writeRawData(char **p, char *d, int len) {
	for (int i = 0; i < len; ++i, (*p)++)
		**p = d[i];
}

/* simple getter and setter */

const char *PConnector::getStreamName() const {
	return streamName;
}

const char *PConnector::getSensorName() const {
	return streamName;
}

int PConnector::getStreamId() const {
	return streamId;
}

int PConnector::getSensorId() const {
	return streamId;
}

/* verboseモードを設定できる(現在は設定しても何も起こらない) */
void PConnector::setVerbose(bool verbose) {
	isVerbose = verbose;
}

/* blockingの設定、今は何も起きない */
void PConnector::setBlocking(bool isBlocking) {
	this->isBlocking = isBlocking;
}

void* PConnector::getData() {
	// mDataのゲッタ. 使用するときはTにキャスト. 返り値はポインタであることに注意.
	return mData;
}

bool PConnector::isOpen() {
	// streamは開いているか？
	return (streamId != 0) ? true : false;
}

void *PConnector::data() {
	return mData;
}

uint64_t PConnector::dataSize() {
	return mDataSize;
}

void *PConnector::property() {
	return mProperty;
}

uint64_t PConnector::propertySize() {
	return mPropertySize;
}

uint64_t PConnector::sharedSize() {
	return mDataSize;
}

SSM_sid PConnector::getSSMId() {
	return 0;
}

SSM_tid PConnector::getTID_top(SSM_sid sid) {
	return timeId = getTID_top();
}

SSM_tid PConnector::getTID_bottom(SSM_sid sid) {
	return timeId = getTID_bottom();
}
//TAKUTO CHANGE
bool PConnector::sendTMsg(thrd_msg *tmsg) {
	char *p = tbuf;
	serializeTMessage(tmsg, &p);
	if (send(dsock, tbuf, thrdMsgLen, 0) == -1) {
		perror("socket error");
		return false;
	}
	return true;
}

bool PConnector::recvTMsg(thrd_msg *tmsg) {
	int len = recv(dsock, tbuf, thrdMsgLen, 0);
	if (len == thrdMsgLen) {
		char* p = tbuf;
		return deserializeTMessage(tmsg, &p);
	}
	return false;
}

SSM_tid PConnector::getTID(SSM_sid sid, ssmTimeT ytime) {
	thrd_msg tmsg;
	memset(&tmsg, 0, sizeof(thrd_msg));

	tmsg.msg_type = TID_REQ;
	tmsg.time = ytime;

	if (!sendTMsg(&tmsg)) {
		return -1;
	}

	if (recvTMsg(&tmsg)) {
		if (tmsg.res_type == TMC_RES) {
			return tmsg.tid;
		}
	}
	return -1;
}

SSM_tid PConnector::getTID_bottom() {
	thrd_msg tmsg;
	memset(&tmsg, 0, sizeof(thrd_msg));

	tmsg.msg_type = BOTTOM_TID_REQ;

	if (!sendTMsg(&tmsg)) {
		return -1;
	}

	if (recvTMsg(&tmsg)) {
		if (tmsg.res_type == TMC_RES) {
			return tmsg.tid;
		}
	}
	return -1;
}

SSM_tid PConnector::getTID_top() {
	thrd_msg tmsg;
	memset(&tmsg, 0, sizeof(thrd_msg));

	tmsg.msg_type = TOP_TID_REQ;

	if (!sendTMsg(&tmsg)) {
		return -1;
	}
	if (recvTMsg(&tmsg)) {
    if (tmsg.res_type == TMC_RES) {
			return tmsg.tid;
    }
	}
	return -1;
}

/* fin getter methods */

bool PConnector::connectToServer(const char* serverName, int port) {

	sock = socket(AF_INET, SOCK_STREAM, 0);

	int flag = 1;
	int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	if (ret == -1) {
		perror("client setsockopt\n");
		exit(1);
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr(serverName);
	if (connect(sock, (struct sockaddr *) &server, sizeof(server))) {
		fprintf(stderr, "connectToServer:connection error\n");
		return false;
	}
	return true;
}

bool PConnector::connectToDataServer(const char* serverName, int port) {
	dsock = socket(AF_INET, SOCK_STREAM, 0);
	int flag = 1;
	int ret = setsockopt(dsock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	
	if (ret == -1) {
		perror("client setsockopt\n");
		exit(1);
	}
	dserver.sin_family = AF_INET;
	dserver.sin_port = htons(port);
	dserver.sin_addr.s_addr = inet_addr(serverName);
	if (connect(dsock, (struct sockaddr *) &dserver, sizeof(dserver))) {
		fprintf(stderr, "connectToDataServer:connection error\n");
		return false;
	}
	return true;
}

bool PConnector::UDPconnectToDataServer(const char* serverName, int port) {
	fprintf(stderr, "Connecting to UDP Data server\n");
	dsock = socket(AF_INET, SOCK_DGRAM, 0);
	int flag = 1;
	dserver.sin_family = AF_INET;
	dserver.sin_port = htons(port);
	dserver.sin_addr.s_addr = inet_addr(serverName);
	if (connect(dsock, (struct sockaddr *) &dserver, sizeof(dserver))) {
		fprintf(stderr, "UDPconnectToDataServer:connection error\n");
		return false;
	}
	send(dsock,0,0,0);
	return true;
}


bool PConnector::initRemote() {
	bool r = true;
	ssm_msg msg;
	char *msg_buf = (char*) malloc(sizeof(ssm_msg));
        
	connectToServer(ipaddr, 8080);
	if (!sendMsgToServer(MC_INITIALIZE, NULL)) {
		fprintf(stderr, "error in initRemote\n");
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		if (msg.cmd_type != MC_RES) {
			r = false;
		}
	} else {
		fprintf(stderr, "fail recvMsg\n");
		r = false;
	} 
	free(msg_buf);
	return r;
}

bool PConnector::initSSM() {
	return initRemote();
}

void PConnector::deserializeMessage(ssm_msg *msg, char *buf) {
	msg->msg_type = readLong(&buf);
	msg->res_type = readLong(&buf);
	msg->cmd_type = readInt(&buf);
	readRawData(&buf, msg->name, 32);
	msg->suid = readInt(&buf);
	msg->ssize = readLong(&buf);
	msg->hsize = readLong(&buf);
	msg->time = readDouble(&buf);
	msg->saveTime = readDouble(&buf);
}

bool PConnector::serializeTMessage(thrd_msg *tmsg, char **p) {
	/*if (tmsg == NULL) {
		return false;
	}*/
	writeLong(p, tmsg->msg_type);
	writeLong(p, tmsg->res_type);
	writeInt(p, tmsg->tid);
	writeDouble(p, tmsg->time);
	return true;
}

bool PConnector::deserializeTMessage(thrd_msg *tmsg, char **p) {
	/*if (tmsg == NULL)
		return false;*/
	tmsg->msg_type = readLong(p);
	tmsg->res_type = readLong(p);
	tmsg->tid = readInt(p);
	tmsg->time = readDouble(p);
	return true;
}

bool PConnector::recvMsgFromServer(ssm_msg *msg, char *buf) {
	int len = recv(sock, buf, dssmMsgLen, 0);
	if (len > 0) {
		deserializeMessage(msg, buf);
		return true;
	}
	return false;
}

/* read */

// 前回のデータの1つ(以上)前のデータを読み込む
bool PConnector::readBack(int dt = 1) {
	return (dt <= timeId ? read(timeId - dt) : false);
}

// 新しいデータを読み込む
bool PConnector::readLast() {
	return read(-1);
}

// 前回のデータの1つ(以上)あとのデータを読み込む
bool PConnector::readNext(int dt = 1) {
	thrd_msg tmsg;
	memset(&tmsg, 0, sizeof(thrd_msg));
	tmsg.msg_type = READ_NEXT;
	tmsg.tid = dt;

	if (!sendTMsg(&tmsg)) {
		return false;
	}
	if (recvTMsg(&tmsg)) {
		if (tmsg.res_type == TMC_RES) {
			if (recvData()) {
				time = tmsg.time;
				timeId = tmsg.tid;
				return true;
			}
		}
	}
	return false;
}

bool PConnector::readNew() {
	return (isOpen() ? read(-1) : false);
}

bool PConnector::readTime(ssmTimeT t) {
	thrd_msg tmsg;
	memset(&tmsg, 0, sizeof(thrd_msg));

	tmsg.msg_type = REAL_TIME;
	tmsg.time = t;

	if (!sendTMsg(&tmsg)) {
		return -1;
	}

	if (recvTMsg(&tmsg)) {
		if (tmsg.tid == -1) {
			fprintf(stderr, "readTime: recv -1");
			return -1;
		}
		if (tmsg.res_type == TMC_RES) {
			if (recvData()) {
				time = tmsg.time;
				timeId = tmsg.tid;
				return true;
			}
		}
	}
	return -1;
}

bool PConnector::read(SSM_tid tmid, READ_packet_type type) {
	thrd_msg tmsg;
	memset((char*)&tmsg, 0, sizeof(thrd_msg));
	tmsg.msg_type = type;
	tmsg.tid = tmid;
	//debug
	if (!sendTMsg(&tmsg)) {
		return false;
	}
	if (recvTMsg(&tmsg)) {
		if (tmsg.res_type == TMC_RES) {
			if (recvData()) {
				time = tmsg.time;
				timeId = tmsg.tid;
				return true;
			}
		}
	}
	return false;
}

/* read ここまで　*/
bool PConnector::recvData() {
	int len = 0;
	len = recv(dsock, &((char*) mData)[len], mDataSize, 0);
	/*
	while ((len += recv(dsock, &((char*) mData)[len], mDataSize - len, 0))
			!= mDataSize)
		;
		*/
	return true;
}

ssmTimeT PConnector::getRealTime() {
	struct timeval current;
	gettimeofday(&current, NULL);
	return current.tv_sec + current.tv_sec / 1000000.0;
}
//送信
bool PConnector::write(ssmTimeT time) {
	*((ssmTimeT*) mFullData) = time;
	if (send(dsock, mFullData, mFullDataSize, 0) == -1) { // データ送信用経路を使う
		perror("send");
		return false;
	}
	this->time = time;
	return true;
}

bool PConnector::sendMsgToServer(int cmd_type, ssm_msg *msg) {
	ssm_msg msgbuf;
	char *buf, *p;
	if (msg == NULL) {
		msg = &msgbuf;
	}
	msg->msg_type = 1; // dummy
	msg->res_type = 8;
	msg->cmd_type = cmd_type;
	buf = (char*) malloc(sizeof(ssm_msg));
	p = buf;
	writeLong(&p, msg->msg_type);
	writeLong(&p, msg->res_type);
	writeInt(&p, msg->cmd_type);
	writeRawData(&p, msg->name, 32);
	writeInt(&p, msg->suid);
	writeLong(&p, msg->ssize);
	writeLong(&p, msg->hsize);
	writeDouble(&p, msg->time);
	writeDouble(&p, msg->saveTime);

	if (send(sock, buf, dssmMsgLen, 0) == -1) {
		fprintf(stderr, "error happens\n");
		free(buf);
		return false;
	}
	free(buf);
	return true;
}

bool PConnector::sendData(const char *data, uint64_t size) {
	if (send(sock, data, size, 0) == -1) {
		fprintf(stderr, "error in sendData\n");
		return false;
	}
	return true;
}

// mDataにfulldataのポインタをセット
void PConnector::setBuffer(void *data, uint64_t dataSize, void *property,
		uint64_t propertySize, void* fulldata) {
	mData = data;
	mDataSize = dataSize;
	mProperty = property;
	mPropertySize = propertySize;
	mFullData = fulldata;
	mFullDataSize = mDataSize + sizeof(ssmTimeT);
}

// IPAddressをセット(デフォルトはループバック)
void PConnector::setIpAddress(const char *address) {
	ipaddr = address;
}

void PConnector::setStream(const char *streamName, int streamId = 0) {
	this->streamName = streamName;
	this->streamId = streamId;
}

bool PConnector::createRemoteSSM(const char *name, int stream_id,
		uint64_t ssm_size, ssmTimeT life, ssmTimeT cycle) {
	ssm_msg msg;
	int open_mode = SSM_READ | SSM_WRITE;
	uint64_t len;

	/* initialize check */
	if (!name) {
		fprintf( stderr,
				"SSM ERROR : create : stream name is NOT defined, err.\n");
		return false;
	}
	len = strlen(name);
	if (len == 0 || len >= SSM_SNAME_MAX) {
		fprintf( stderr,
				"SSM ERROR : create : stream name length of '%s' err.\n", name);
		return false;
	}

	if (stream_id < 0) {
		fprintf( stderr, "SSM ERROR : create : stream id err.\n");
		return false;
	}

	if (life <= 0.0) {
		fprintf( stderr, "SSM ERROR : create : stream life time err.\n");
		return false;
	}

	if (cycle <= 0.0) {
		fprintf( stderr, "SSM ERROR : create : stream cycle err.\n");
		return false;
	}

	if (life < cycle) {
		fprintf( stderr,
				"SSM ERROR : create : stream saveTime MUST be larger than stream cycle.\n");
		return false;
	}

	strncpy(msg.name, name, SSM_SNAME_MAX);
	msg.suid = stream_id;
	msg.ssize = ssm_size;
	msg.hsize = calcSSM_table(life, cycle); /* table size */
	msg.time = cycle;
	msg.saveTime = life;

	bool r = true;
	char *msg_buf = (char*) malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_CREATE | open_mode, &msg)) {
		fprintf(stderr, "error in createRemoteSSM\n");
		r = false;
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
//		printf("msg %d\n", (int) msg.cmd_type);
		if (msg.cmd_type != MC_RES) {
			fprintf(stderr, "MC_CREATE Remote RES is not MC_RES\n");
			r = false;
		}
	} else {
		fprintf(stderr, "fail recvMsg\n");
		r = false;
	}
	free(msg_buf);
	return r;
}

bool PConnector::open(SSM_open_mode openMode) {
	this->openMode = openMode;
	ssm_msg msg;
	std::cout<< "IN OPEN" << std::endl;
	if (!mDataSize) {
		std::cerr << "ssm-proxy-client: data buffer of" << streamName
				<< "', id = " << streamId << " is not allocked." << std::endl;
		return false;
	}

	// メッセージにストリームIDとストリームネームをセット
	msg.suid = streamId;
	strncpy(msg.name, streamName, SSM_SNAME_MAX);
	msg.ssize = mDataSize;
	// ssmOpen( streamName, streamId, openMode )の実装
	// 内部のcommunicatorでMC_OPENを発行。
	// PConnector::sendMsgToServer(int cmd_type, ssm_msg *msg)を実装
	if (!sendMsgToServer(MC_OPEN | (int) openMode, &msg)) {
		fprintf(stderr, "PConnector::open error send\n");
		return false;
	}

	// proxyからのメッセージを受信
	char *msg_buf = (char*) malloc(sizeof(ssm_msg));
	if (recvMsgFromServer(&msg, msg_buf)) {
		if (!(msg.cmd_type == MC_RES)) {
			fprintf(stderr, "PConnector::open error recv\n");
			return false;
		}
	}

	return true;
}

bool PConnector::open(const char *streamName, int streamId = 0,
		SSM_open_mode openMode = SSM_READ) {
	setStream(streamName, streamId);
	return open(openMode);
}

bool PConnector::create(double saveTime, double cycle) {
	if (!mDataSize) {
		std::cerr << "SSM::create() : data buffer of ''" << streamName
				<< "', id = " << streamId << " is not allocked." << std::endl;
		return false;
	}

	return this->createRemoteSSM(streamName, streamId, mDataSize, saveTime,
			cycle);
}
bool PConnector::create(const char *streamName, int streamId, double saveTime,
		double cycle) {
	setStream(streamName, streamId);
	return create(saveTime, cycle);
}

bool PConnector::setProperty() {
	if (mPropertySize > 0) {
		return setPropertyRemoteSSM(streamName, streamId, mProperty,
				mPropertySize);
	}
	return false;
}

bool PConnector::setPropertyRemoteSSM(const char *name, int sensor_uid,
		const void *adata, uint64_t size) {
	ssm_msg msg;
	const char *data = (char *) adata;
	if (strlen(name) > SSM_SNAME_MAX) {
		fprintf(stderr, "name length error\n");
		return 0;
	}

	/* メッセージをセット */
	strncpy(msg.name, name, SSM_SNAME_MAX);
	msg.suid = sensor_uid;
	msg.ssize = size;
	msg.hsize = 0;
	msg.time = 0;

	bool r = true;
	char *msg_buf = (char*) malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_STREAM_PROPERTY_SET, &msg)) {
		fprintf(stderr, "error in setPropertyRemoteSSM\n");
		r = false;
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		if (msg.cmd_type != MC_RES) {
			//r = false;
			return false;
		}

		if (!sendData(data, size)) {
			r = false;
		}
		if (recvMsgFromServer(&msg, msg_buf)) {			
			if (msg.cmd_type != MC_RES) {
				r = false;
			}
		}

	} else {
		fprintf(stderr, "fail recvMsg\n");
		r = false;
	}
	free(msg_buf);
	return r;
}

bool PConnector::getProperty() {
	if (mPropertySize > 0) {
		return getPropertyRemoteSSM(streamName, streamId, mProperty);
	}
	return false;
}

bool PConnector::getPropertyRemoteSSM(const char *name, int sensor_uid,
		const void *adata) {
	ssm_msg msg;
	char *data = (char *) adata;
	if (strlen(name) > SSM_SNAME_MAX) {
		fprintf(stderr, "name length error\n");
		return 0;
	}

	strncpy(msg.name, name, SSM_SNAME_MAX);
	msg.suid = sensor_uid;
	msg.ssize = mPropertySize;
	msg.hsize = 0;
	msg.time = 0;

	bool r = true;
	char *msg_buf = (char*) malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_STREAM_PROPERTY_GET, &msg)) {
		fprintf(stderr, "error in getPropertyRemoteSSM send\n");
		return false;
	}

	if (recvMsgFromServer(&msg, msg_buf)) {
		if (msg.cmd_type != MC_RES) {
			fprintf(stderr, "error in getPropertyRemoteSSM recv\n");
			return false;
		}
		// property取得
		uint64_t len = 0;
		while ((len += recv(sock, &data[len], mPropertySize - len, 0))
				!= mPropertySize)
			;
		if (len != mPropertySize) {
			fprintf(stderr, "fail recv property\n");
			r = false;
		}
	} else {
		fprintf(stderr, "fail recvMsg\n");
		r = false;
	}
	free(msg_buf);
	return r;
}

void PConnector::setOffset(ssmTimeT offset) {
	ssm_msg msg;
	msg.hsize = 0;
	msg.ssize = 0;
	msg.suid = 0;
	msg.time = offset;
	char *msg_buf = (char*) malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_OFFSET, &msg)) {
		fprintf(stderr, "error in setOffset\n");
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		fprintf(stderr, "msg res offset %d\n", (int) msg.cmd_type);
	}
	free(msg_buf);
}

bool PConnector::createDataCon() {
	ssm_msg msg;
	msg.hsize = 0;
	msg.ssize = 0;
	msg.suid = 0;
	msg.time = 0;
	char *msg_buf = (char*) malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_CONNECTION, &msg)) {
		fprintf(stderr, "error in createDataCon\n");
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
//            fprintf(stderr, "create data connection error\n");
	}
	free(msg_buf);
	connectToDataServer(ipaddr,msg.suid);
	return true;
	}

bool PConnector::UDPcreateDataCon() {
	ssm_msg msg;
	msg.hsize = 0;
	msg.ssize = 0;
	msg.suid = 0;
	msg.time = 0;
	char *msg_buf = (char*) malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_UDPCONNECTION, &msg)) {
		fprintf(stderr, "error in createDataCon\n");
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
	//fprintf(stderr, "create data connection error\n");
	}

	free(msg_buf);
	UDPconnectToDataServer(ipaddr, msg.suid);
	return true;
}

bool PConnector::release() {
	return terminate();
}

bool PConnector::terminate() {
	ssm_msg msg;
	msg.hsize = 0;
	char *msg_buf = (char*) malloc(sizeof(ssm_msg));
	if (!sendMsgToServer(MC_TERMINATE, &msg)) {
		fprintf(stderr, "error in setOffset\n");
	}
	if (recvMsgFromServer(&msg, msg_buf)) {
		fprintf(stderr, "msg terminate %d\n", (int) msg.cmd_type);
	}
	free(msg_buf);

	return true;
}