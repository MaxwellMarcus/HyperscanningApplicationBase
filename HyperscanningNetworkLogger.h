#ifndef NETWORK_LOGGER_H
#define NETWORK_LOGGER_H

#include "Environment.h"
#include "Thread.h"
#include <atomic>
#include <netinet/in.h>
#include <vector>
#include <queue>
#include <mutex>

class HyperscanningNetworkThread : public Thread {
	public:
	private:
};

class HyperscanningNetworkLogger : public EnvironmentExtension, public Thread {
	public:
		HyperscanningNetworkLogger();
		~HyperscanningNetworkLogger();

		void Publish() override;
		void Preflight() const override;
		void Initialize() override;
		void StartRun() override;
		void Process() override;
		void StopRun() override;
		void Halt() override;

		int OnExecute() override;
		void Setup();

		void Interpret( char* );

	private:
		bool mLogNetwork;

		std::atomic<bool> mTerminate;

		std::vector<std::string> mSharedStates;
		std::string mMessage;
		std::mutex mMessageMutex;

		int sockfd;
		int clientfd;
		sockaddr_in serv_addr;
		char* buffer;

		char* mAddress;
		int mPort = 1234;
};

#endif
