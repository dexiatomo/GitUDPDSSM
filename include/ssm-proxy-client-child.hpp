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
	void setRBuffer(int n)
	{
		ringBuf.setBufferSize(n);
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
		//std::cout << __PRETTY_FUNCTION__ << std::endl;
		free(fulldata);
		free(wdata);
	}

	//TAKUTO
	void rBufReadTask()
	{
		struct pollfd polldata;
		bool loop = true;
		int cnt = 0;
		polldata.events = POLLIN;
		polldata.revents = 0;
		polldata.fd = dsock;
		std::vector<char *> recvBuf(sizeof(T) + sizeof(ssmTimeT) + sizeof(SSM_tid));
		ssmData ssmRecvData;
		while (loop)
		{
			switch (poll(&polldata, 1, 1000))
			{
			case -1:
				perror("poll");
				break;

			case 0:
				std::cout << "no data yet" << std::endl;
				cnt++;
				if (cnt > 10)
					loop = false;
				break;

			default:
				recv(dsock, &recvBuf, mFullDataSize + 4, 0);
				int homeTID = readInt(&recvBuf[0]);
				ssmRecvData.time = readDouble(&recvBuf[4]);
				readRawData(&recvBuf[12], (char *)&ssmRecvData.ssmRawData, sizeof(T));
				std::cout << "received data " << std::endl;
				loop = false;
				ringBuf.writeBuffer(ssmRecvData.ssmRawData);
				break;
			};
		}
	}

	std::thread readyRingBuf(int bufnum)
	{
		//スレッドスタート
		std::cout << "Starting Ring Buffer" << std::endl;
		setRBuffer(bufnum);
		std::thread readThread([this]{rBufReadTask();});
		readThread.join();
	}
};

#endif
