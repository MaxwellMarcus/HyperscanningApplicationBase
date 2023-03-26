#include "HyperscanningNetworkLogger.h"
#include "BCIStream.h"
#include "Thread.h"
#include "BCIEvent.h"

#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#if _WIN32
#include <Winsock2.h>
#undef errno
#define errno WSAGetLastError()
#else
#include <sys/socket.h>
#endif

Extension( HyperscanningNetworkLogger );

HyperscanningNetworkLogger::HyperscanningNetworkLogger()
: mBuffer(nullptr), mLogNetwork( false ) {
}

HyperscanningNetworkLogger::~HyperscanningNetworkLogger() {
	::free(mBuffer);
	Halt();
}

void HyperscanningNetworkLogger::Publish() {
	if ( OptionalParameter( "LogNetwork" ) > 0 ) {
		BEGIN_PARAMETER_DEFINITIONS
			"Source:Hyperscanning%20Network%20Logger int LogNetwork= 1 0 0 1"
			" // record hyperscanning network states (boolean) ",
			"Source:Hyperscanning%20Network%20Logger string IPAddress= 10.138.1.182 % % %"
			" // IPv4 address of server",
			"Source:Hyperscanning%20Network%20Logger int Port= 1234 % % %"
			" // server port",
			"Source:Hyperscanning%20Network%20Logger string ParameterPath= ../parms/CommunicationTask/HyperScanningParameters.prm % % %"
			" // IPv4 address of server",
			//"Source:Hyperscanning%20Network%20Logger string SharedStates= % % % % //shared states"
		END_PARAMETER_DEFINITIONS

		BEGIN_EVENT_DEFINITIONS
			"ClientNumber 8 0 0 0"
		END_EVENT_DEFINITIONS

		//Parameter( "SharedStates" ) = "TrialNumber,8&PhaseNumber,8&BarOneValue,8&BarTwoValue,8&BarOneActive,8&BarTwoActive,8&ResponseValue,8&SenderLock,8&ReceiverLock,8&isReadyStart0,8&isReadyStart1,8";
		std::string states( Parameter( "SharedStates" ) );
		std::istringstream f( ( states ) );
		std::string name;
		std::string size;
		std::vector<std::string> sharedStates;
		int i = 0;
		while ( getline( f, name, ',' ) ) {
			if ( !getline( f, size, '&' ) )
				bcierr << "Every Shared State must have a size";
			bciwarn << "Shared State: " << name << ", " << size;
			BEGIN_EVENT_DEFINITIONS
				name + " " + size + " 0 0 0"
			END_EVENT_DEFINITIONS

			sharedStates.push_back( name );
		}
		mSharedStates = sharedStates;
		mStateValues = std::vector<uint64_t>( mSharedStates.size(), 0 );
		mHasUpdated = std::vector<bool>( mSharedStates.size(), true );

		if (OptionalParameter("LogNetwork") > 0)
			Setup();
	}
}
//
void HyperscanningNetworkLogger::Preflight() const {
	if ( OptionalParameter( "LogNetwork" ) > 0 ) {
		if ( Parameter( "SharedStates" )->NumValues() < 1 )// || Parameter( "SharedStates" )->NumValues() % 2 == 1 )
			bcierr << "You must have at least one shared state and a name and size for each";
		if ( !OptionalParameter( "IPAddress" ) )
			bcierr << "Must give server address";
		if ( !OptionalParameter( "Port" ) )
			bcierr << "Must specify port";
		if (((std::string)Parameter("ParameterPath")).empty()) {
			bcierr << "Must give parameter path";
		}
	}

}

void HyperscanningNetworkLogger::Initialize() {
	mLogNetwork = ( OptionalParameter( "LogNetwork" ) > 0 );
	if (!mLogNetwork) return;
	bciwarn << "Client Number: " << ( int ) mClientNumber;
	State( "ClientNumber" ) = mClientNumber;

}

void HyperscanningNetworkLogger::AutoConfig() {
	if (OptionalParameter("LogNetwork") > 0){
		if ( downloadedParms )
			Parameters->Load((std::string)Parameter("ParameterPath"), false);
	}
}

void HyperscanningNetworkLogger::StartRun() {
	if (mLogNetwork) {
		bciwarn << "Starting Network Thread";
		Start();
	}
}

void HyperscanningNetworkLogger::Process() {
	if ( !mLogNetwork ) return;

	const std::lock_guard<std::mutex> lock( mMessageMutex );
	const std::lock_guard<std::mutex> lock2( mStateValuesMutex );
	//mMessage = "";
	//bciwarn << "Checking for states that need to be updated";
	for ( int i = 0; i < mSharedStates.size(); i++ ) {
		if ( !mHasUpdated[ i ] ) {
			bciwarn << "Updated state other client changed";
			bciwarn << "State: " << mSharedStates[ i ];
			bciwarn << "State Size: " << State( mSharedStates[ i ] )->Length();
			bciwarn << "Value Size: " << sizeof( mStateValues[ i ] );
			bciwarn << "Value: " << mStateValues[ i ];
			State( mSharedStates[ i ] ) = mStateValues[ i ];
			bciwarn << "Has Updated";
			mHasUpdated[ i ] = true;
		}
		std::string message( mSharedStates[ i ] );
		StateRef s = State( mSharedStates[ i ] );
		uint64_t val = s;
		if ( val != mStateValues[ i ] ) {
			bciwarn << val << " != " << mStateValues[ i ];
			bciwarn << mSharedStates[ i ] << " now is " << val;
			message.push_back( '\0' );
			message.push_back( ( char ) s->Length() / 8 );
			message += std::string( ( char* )( &val ), s->Length() / 8 );

			mMessage += message;

			mStateValues[ i ] = val;
		}
	}
}
//
void HyperscanningNetworkLogger::StopRun() {
	if (!mLogNetwork) return;
	bciwarn << "Stop Run";
	TerminateAndWait();
}

void HyperscanningNetworkLogger::Halt() {
	if (!mLogNetwork) return;
	TerminateAndWait();
}

//int HyperscanningNetworkLogger::on_create() {
//	return m_fd = ::socket( AF_INET, SOCK_STREAM, 0 );
//}


void HyperscanningNetworkLogger::Setup() {
	//mAddress = "10.138.1.182";
	//mAddress = "127.0.0.1";
	//mAddress = "172.20.10.10";
	mAddress = ( std::string )OptionalParameter( "IPAddress" );
	mPort = OptionalParameter( "Port" );

	bciwarn << "Connecting to " << mAddress << ":" << ( unsigned short )mPort;
	bciout << "Open: ";
	mSocket.Open( mAddress, ( unsigned short )mPort );
	bciout << "Is open: " << mSocket.IsOpen();
	bciout << "Address: " << mSocket.Address();
	bciout << "Connected: " << mSocket.Connected();

	bciout << "Awaiting server greeting...";

	std::string param_file;
	mSocket.Wait();
	mBuffer = ( char* ) calloc( sizeof( size_t ), 1 );
	if ( ::recv(mSocket.Fd(), mBuffer, sizeof( size_t ), 0) < 0 ) {  // read one packet only
		bciwarn << "Error reading: " << errno;
	}
	for ( int i = 0; i < sizeof( size_t ); i++ ) {
		bciwarn << ( int )mBuffer[ i ];
	}
	size_t size = 0;
	memcpy( &size, mBuffer, sizeof( size_t ) );
	free( mBuffer );
	bciwarn << sizeof( size_t );
	bciwarn << "Size: " << size;


	if ( size > 1 ) {
		mBuffer = ( char* )malloc( size );

		for ( int i = 0; i < size; i++ ) {
			mSocket.Wait();
			if ( ::recv(mSocket.Fd(), mBuffer + i, 1, 0) < 0 ) {  // read one packet only
				bciwarn << "Error reading: " << errno;
			}
		}
		
		bciwarn << mBuffer;
		bciwarn << ( int ) mBuffer[ size - 1 ];
		bciwarn << "THATS BUFFER";
		param_file += std::string( mBuffer, size - 1 );

		std::ofstream outfile ((std::string)Parameter("ParameterPath"));
		outfile << param_file;

		downloadedParms = true;

		free( mBuffer );

	}
	bciout << "Recieved server greeting...";

	mBuffer = ( char* ) calloc( 1025, 1 );
	mSocket.Wait();
	//memset( mBuffer, 0, 1025 );
	if ( ::recv(mSocket.Fd(), mBuffer, 1025, 0 ) < 0 ) // read one packet only
		bciwarn << "reading socket: " << errno;

	char* buffer = mBuffer;
	while ( *buffer != 0 ) {
		std::string name( buffer );
		buffer += name.size() + 1;
		bciwarn << "Name: " << name;
		char size = *buffer++;
		std::string value( buffer, size );
		buffer += size;

		uint64_t val = 0;
		memcpy( &val, value.c_str(), size );

		bciwarn << "ClientNumber: " << ( int )val;

		mClientNumber = val;
	}
}

int HyperscanningNetworkLogger::OnExecute() {
	if (mLogNetwork){
		while ( !Terminating() ) {
			free( mBuffer );

			{
				std::lock_guard<std::mutex> messageLock(mMessageMutex);
				std::string name(mMessage.c_str());
				char size = *(mMessage.c_str() + name.size() + 1);
				char value = *(mMessage.c_str() + name.size() + 2);

				mMessage.push_back('\0');

				//bciwarn << "Writing to server...";
				//bciwarn << "Size: " << mMessage.size();
				//bciwarn << "Message: " << mMessage;

				std::string out;

				for ( char i : mMessage ) {
					out += std::to_string( ( int ) i );
					out += ", ";
				}
				//bciwarn << out;
				if (mSocket.Write(mMessage.c_str(), mMessage.size()) < 0)
				{
					bciwarn << "Error writing to socket: " << errno;
					return -1;
				}

				mMessage = std::string( "" );
			}

			if (mSocket.Wait()) // will return false when thread is terminating
			{
				//bciwarn << "Reading from server...";
				mBuffer = ( char* ) calloc( sizeof( size_t ), 1 );
				if ( ::recv(mSocket.Fd(), mBuffer, sizeof( size_t ), 0) < 0 ) {  // read one packet only
					bciwarn << "Error reading: " << errno;
				}
				size_t size = 0;
				memcpy( &size, mBuffer, sizeof( size_t ) );
				//bciwarn << "size: " << size;
				free( mBuffer );
				mBuffer = ( char* ) calloc( size + 1, 1 );

				for ( int i = 0; i < size; i++ ) {
					mSocket.Wait();
					if (::recv(mSocket.Fd(), mBuffer + i, 1, 0) < 0) // read one packet only
					{
						bciwarn << "Error reading socket: " << errno;
						return -1;
					}
				}
				//bciwarn << "Buffer: " << mBuffer;
				std::string out;
				for ( int i = 0; i < size; i++ ) {
					out += std::to_string( ( int ) mBuffer[ i ] );
					out += ", ";
				}
				//bciwarn << out;
				Interpret(mBuffer);
			}
		}
	}
	return 1;
}

void HyperscanningNetworkLogger::Interpret( char* buffer ) {
	while ( *buffer != 0 ) {
		std::string name( buffer );
		buffer += name.size() + 1;
		char size = *buffer++;
		bciwarn << "Incoming value size: " << ( int )size;
		if ( size == 0 ) continue;
		std::string value( buffer, size );
		buffer += size;

		uint64_t val = 0;
		memcpy( &val, value.c_str(), size );

		bciwarn << name << ": " << val;

		mStateValuesMutex.lock();
		auto it = find( mSharedStates.begin(), mSharedStates.end(), name );
		if ( it != mSharedStates.end() ) {
			mStateValues[ it - mSharedStates.begin() ] = val;
			mHasUpdated[ it - mSharedStates.begin() ] = false;
		} else {
			bciwarn << "State is not a part of Hyperscanning Shared States";
		}
		mStateValuesMutex.unlock();

		//bcievent << name << " " << val;
	}
}

