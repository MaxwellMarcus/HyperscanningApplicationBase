#ifndef NETWORK_LOGGER_H
#define NETWORK_LOGGER_H

#include "ApplicationBase.h"
#include "Thread.h"
#include "Sockets.h"
#include <atomic>
#include <mutex>

class HyperscanningNetworkThread : public Thread {
	public:
	private:
};

class HyperscanningNetworkLogger : public ApplicationBase, public Thread {
	public:
		HyperscanningNetworkLogger();
		~HyperscanningNetworkLogger();

		virtual void Publish() override;
		virtual void AutoConfig( const SignalProperties& Input ) override;
		virtual void Preflight( const SignalProperties& Input, SignalProperties& Output ) const override;
		virtual void Initialize( const SignalProperties& Input, const SignalProperties& Output ) override;
		virtual void StartRun() override;
		void UpdateStates();
		void UpdateMessage();
		virtual void StopRun() override;
		virtual void Halt() override;

		int OnExecute() override;
		void Setup();

		void Interpret( char* );

		size_t GetServerMessageSize();
		void GetServerMessage( char*, size_t );

	private:

		//int on_create() override;
//		int on_open() override {
//		    return 1;
//		}

		bool downloadedParms = false;

		bool mLogNetwork;

		std::vector<std::string> mSharedStates;
		std::vector<uint64_t> mStateValues;
		std::vector<bool> mHasUpdated;
		std::mutex mStateValuesMutex;
		std::string mMessage;
		std::mutex mMessageMutex;

		ClientTCPSocket mSocket;
/*		int sockfd;
		int clientfd; */
		char* mBuffer;

		std::string mAddress;
		int mPort;

		char mClientNumber;
};

#endif
