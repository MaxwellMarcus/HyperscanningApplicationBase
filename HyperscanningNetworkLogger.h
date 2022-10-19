#ifndef NETWORK_LOGGER_H
#define NETWORK_LOGGER_H

#include "Environment.h"
#include "Thread.h"
#include "sockstream.h"
#include <atomic>
#include <netinet/in.h>
#include <vector>
#include <queue>
#include <mutex>

class HyperscanningNetworkThread : public Thread {
	public:
	private:
};

class HyperscanningNetworkLogger : public EnvironmentExtension, public Thread, public sockio::socket {
	public:
		HyperscanningNetworkLogger();
		~HyperscanningNetworkLogger();

		void Publish() override;
		void AutoConfig() override;
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

		int on_create() override;
		int on_open() override {
		}

		bool mLogNetwork;

		std::atomic<bool> mTerminate;

		std::vector<std::string> mSharedStates;
		std::vector<uint32_t> mPreviousStates;
		std::vector<bool> mHasUpdated;
		std::mutex mPreviousStatesMutex;
		std::string mMessage;
		std::mutex mMessageMutex;

		int sockfd;
		int clientfd;
		sockaddr_in serv_addr;
		char* buffer;

		std::string mAddress;
		int mPort;

		char ClientNumber;
};

#endif
