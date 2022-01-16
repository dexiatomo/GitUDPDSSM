// テンプレートを利用した実装はヘッダに書くこと(リンカに怒られる)
/*
 * SSMの宣言に仕様する
 * ほとんどSSMApiと同じように使用できる
 */

#ifndef _SSM_PROXY_CLIENT_CHILD_
#define _SSM_PROXY_CLIENT_CHILD_

#include "ssm-proxy-client.hpp"
#include "dssm-utility.hpp"
#include <sys/poll.h>
#include <vector>
#include <iostream>
#include <thread>
#include <functional>

template <typename T, typename P = DSSMDummy>
class PConnectorClient : public PConnector
{
private:
	dssm::rbuffer::RingBuffer<T> ringBuf;
	struct ssmData
	{
		ssmTimeT time;
		T ssmRawData;
	};
	void initApi(char *ipAddr)
	{
		fulldata = malloc(sizeof(T) + sizeof(ssmTimeT)); // メモリの開放はどうする？ -> とりあえずデストラクタで対応
		wdata = (T *)&(((char *)fulldata)[8]);
		PConnector::setBuffer(&data, sizeof(T), &property, sizeof(P), fulldata);
		PConnector::setIpAddress(ipAddr);
	}

	void initApi()
	{
		fulldata = malloc(sizeof(T) + sizeof(ssmTimeT)); // メモリの開放はどうする？ -> とりあえずデストラクタで対応
		wdata = (T *)&(((char *)fulldata)[8]);
		PConnector::setBuffer(&data, sizeof(T), &property, sizeof(P), fulldata);
	}

protected:
	void setBuffer(void *data, uint64_t dataSize, void *property, uint64_t propertySize, void *fulldata);

public:
	T data;
	T *wdata;
	P property;
	void *fulldata;

	PConnectorClient()
	{
		initApi();
	}
	// 委譲
	PConnectorClient(const char *streamName, int streamId = 0, char *ipAddr = "127.0.0.1") : PConnector::PConnector(streamName, streamId)
	{
		initApi(ipAddr);
	}
	//デストラクタ
	~PConnectorClient()
	{
		// std::cout << __PRETTY_FUNCTION__ << std::endl;
		free(fulldata);
		free(wdata);
	}

	// TAKUTO
	void rBufReadTask()
	{
		std::cout << "rBufReadTask start." << std::endl;
		struct pollfd polldata;
		bool loop = true;
		int cnt = 0;
		polldata.events = POLLIN;
		polldata.revents = 0;
		polldata.fd = dsock;
		std::vector<char> recvBuf(sizeof(T) + sizeof(ssmTimeT) + sizeof(SSM_tid));
		ssmData ssmRecvData;
		while (loop)
		{
			switch (poll(&polldata, 1, 10))
			{
			case -1:
				perror("poll");
				break;
			case 0:
				break;
			default:
				ssize_t recvsize = recv(dsock, recvBuf.data(), mFullDataSize + 4, 0);
				char *p = recvBuf.data();
				SSM_tid tid = readInt(&p);
				ssmTimeT time = readDouble(&p);
				T toridata = *(reinterpret_cast<T *>(p));
				ringBuf.writeBuffer(toridata, tid, time);
				// int homeTID = readInt(&recvBuf[0]);
				/*
				ssmRecvData.time = readDouble(&recvBuf[4]);
				readRawData(&recvBuf[12], (char *)&ssmRecvData.ssmRawData, sizeof(T));
				std::cout << "received data " << std::endl;
				loop = false;
				ringBuf.writeBuffer(ssmRecvData.ssmRawData);
				*/
				break;
			};
		}
	}

	void readyRingBuf(int bufnum)
	{
		//スレッドスタート
		std::cout << "Starting Ring Buffer" << std::endl;
		ringBuf.setBufferSize(bufnum);
		std::cout << "RBuf is ready" << std::endl;
		std::thread readThread([this]{ rBufReadTask(); });
		std::cout << "Created Thread." << std::endl;
		readThread.detach();
	}

	/* read */
	bool readBuf(SSM_tid tid_in = -1)
	{
		switch (ringBuf.read(tid_in, this->data, this->timeId, this->time))
		{
		case 1: // Success
			break;
		case SSM_ERROR_FUTURE:
			std::cout << "SSM_ERROR_FUTURE" << std::endl;
			break;
		case SSM_ERROR_PAST:
			std::cout << "SSM_ERROR_PAST" << std::endl;
			//return read(tid_in);
			return false;
			break;
		case SSM_ERROR_NO_DATA:
			std::cout << "SSM_ERROR_NO_DATA" << std::endl;
			//return read(tid_in);
			return false;
			break;
		}
		return true;
	}

	/** @brief 最新であり、前回読み込んだデータと違うデータのときに読み込む
	 * @return データを読み込めたときtrueを返す
	 */
	bool readNewBuf()
	{
		return (isOpen() ? readBuf(-1) : false);
	}

	/** @brief 前回読み込んだデータの次のデータを読み込む
	 * @param[in] dt 進む量
	 * @return 次データを読み込めたときtrueを返す
	 *
	 * @details 指定するデータがSSMの保存しているデータよりも古いとき、
	 * 保存されている中で最も古いデータを読み込む
	 */
	bool readNextBuf(int dt = 1)
	{
		int tid_in = this->timeId + dt;
		switch (ringBuf.read(tid_in, this->data, this->timeId, this->time))
		{
		case 1: // Success
			break;
		case SSM_ERROR_FUTURE:
			std::cout << "BUFFER ERROR FUTURE" << std::endl;
			break;
		case SSM_ERROR_PAST:
			std::cout << "BUFFER ERROR PAST" << std::endl;
			return read(tid_in);
			break;
		case SSM_ERROR_NO_DATA:
			return read(tid_in);
			break;
		}
		return true;
	}

	// 前回のデータの1つ(以上)前のデータを読み込む
	bool readBackBuf(int dt = 1)
	{
		return (dt <= timeId ? readBuf(timeId - dt) : false);
	}

	// 新しいデータを読み込む
	bool readLastBuf()
	{
		return readBuf(-1);
	}

	bool readTimeBuf(ssmTimeT ytime)
	{
		SSM_tid tid;

		/* 時刻テーブルからTIDを検索 */
		if (ytime <= 0)
		{ /* timeが負の時は最新データの読み込みとする */
			tid = -1;
		}
		else
		{
			switch (ringBuf.getTID(ytime, tid))
			{
			case 1: // 成功
				break;
			case SSM_ERROR_FUTURE:
				//return read(tid);
				break;
			case SSM_ERROR_PAST:
				//return readTime(ytime);
				std::cout << "getTID ERROR PAST" << std::endl;
				return false;
				break;
			default:
				tid = -1;
				break;
			}
		}
		return readBuf(tid);
	}

	SSM_tid getTID_topBuf(SSM_sid sid){
		return ringBuf.getTID_top();
	}
};

#endif
