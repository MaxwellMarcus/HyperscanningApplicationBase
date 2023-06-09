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
#include <stdio.h>

#if _WIN32
#include <Winsock2.h>
#undef errno
#define errno WSAGetLastError()
#else
#include <sys/socket.h>
#endif

//Extension( HyperscanningNetworkLogger );



HyperscanningNetworkLogger::HyperscanningNetworkLogger()
: mBuffer(nullptr), mLogNetwork( false ) {
}



HyperscanningNetworkLogger::~HyperscanningNetworkLogger() {
	::free(mBuffer);
	Halt();
}



//
// Get the size of the message from the server
//

size_t HyperscanningNetworkLogger::GetServerMessageSize() {
	size_t size = 0;

	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = 5000;

	fd_set readfds;
	FD_ZERO( &readfds );
	FD_SET( mSocket.Fd(), &readfds );

	if ( select( mSocket.Fd() + 1, &readfds, NULL, NULL, &time ) ) {
		if ( mSocket.Wait() ) {
			mBuffer = ( char* ) calloc( sizeof( size_t ), 1 );

			if ( ::recv(mSocket.Fd(), mBuffer, sizeof( size_t ), 0) < 0 ) {  // read one packet only
				bciwarn << "Error reading: " << errno;
			}

			memcpy( &size, mBuffer, sizeof( size_t ) );

			free( mBuffer );
		}
	}

	return size;
}



//
// Get the message from the server
//

void HyperscanningNetworkLogger::GetServerMessage( char* buff, size_t size ) {
	for ( int i = 0; i < size; i++ ) {
		if ( mSocket.Wait() ) {
			if ( ::recv(mSocket.Fd(), buff + i, 1, 0) < 0 ) {  // read one packet only
				bciwarn << "Error reading: " << errno;
			}
		}
	}
}



void HyperscanningNetworkLogger::Publish() {
	if ( OptionalParameter( "LogNetwork" ) > 0 ) {

		//
		// Initialize Parameters and local States
		//

		BEGIN_PARAMETER_DEFINITIONS
			"Source:Hyperscanning%20Network%20Logger int LogNetwork= 1 0 0 1"
			" // record hyperscanning network states (boolean) ",
			"Source:Hyperscanning%20Network%20Logger string IPAddress= 10.138.1.104 % % %"
			" // IPv4 address of server",
			"Source:Hyperscanning%20Network%20Logger int Port= 1234 % % %"
			" // server port",
			"Source:Hyperscanning%20Network%20Logger string ParameterPath= ../parms/CommunicationTask/HyperScanningParameters.prm % % %"
			" // IPv4 address of server",
		END_PARAMETER_DEFINITIONS

		BEGIN_STATE_DEFINITIONS
			"ClientNumber 8 0 0 0"
		END_STATE_DEFINITIONS

		//
		// Initialize the states shared with the server and other client
		//

		std::string states( Parameter( "SharedStates" ) ); // get the list of shared states

		std::vector<std::string> sharedStates; // Save the names of each of the states

		std::istringstream f( ( states ) );
		std::string name;
		std::string size;

		while ( getline( f, name, ',' ) ) {

			if ( !getline( f, size, '&' ) )
				bcierr << "Every Shared State must have a size";

			bciout << "Shared State: " << name << ", " << size;

			BEGIN_STATE_DEFINITIONS
				name + " " + size + " 0 0 0"
			END_STATE_DEFINITIONS

			sharedStates.push_back( name );
		}

		mSharedStates = sharedStates;
		mStateValues = std::vector<uint64_t>( mSharedStates.size(), 0 );
		mHasUpdated = std::vector<bool>( mSharedStates.size(), true );

		remove( "../parms/CommunicationTask/HyperScanningParameters.prm" );
	}
}



void HyperscanningNetworkLogger::Preflight( const SignalProperties& Input, SignalProperties& Output ) const {
	if ( OptionalParameter( "LogNetwork" ) > 0 ) {
		if ( Parameter( "SharedStates" )->NumValues() < 1 )
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



void HyperscanningNetworkLogger::Initialize(const SignalProperties& Input, const SignalProperties& Output) {
	mLogNetwork = ( OptionalParameter( "LogNetwork" ) > 0 );
	if (!mLogNetwork) return;
	bciout << "Client Number: " << ( int ) mClientNumber;
	State( "ClientNumber" ) = mClientNumber;
}



void HyperscanningNetworkLogger::AutoConfig(const SignalProperties& Input) {
	bciwarn << "this autoconfig";
	if (OptionalParameter("LogNetwork") > 0){
		Setup();
	}
}



void HyperscanningNetworkLogger::StartRun() {
	if (mLogNetwork) {
		bciwarn << "Starting Network Thread";
		State( "ClientNumber" ) = mClientNumber;
		Start();
	}
}



//void HyperscanningNetworkLogger::Process(const GenericSignal& Input, GenericSignal& Output) {
//	if ( !mLogNetwork ) return;

void HyperscanningNetworkLogger::UpdateStates() {
	// Ensure exclusive access to vectors
	const std::lock_guard<std::mutex> lock( mMessageMutex );
	const std::lock_guard<std::mutex> lock2( mStateValuesMutex );


	for ( int i = 0; i < mSharedStates.size(); i++ ) {

		//
		// Update the local state if the server state has changed
		//

		if ( !mHasUpdated[ i ] ) {

			bciout << "Updated " << mSharedStates[ i ] << " to " << mStateValues[ i ] << " from the server";
			bciout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
			State( mSharedStates[ i ] ) = mStateValues[ i ];
			mHasUpdated[ i ] = true;

		}
	}
}

void HyperscanningNetworkLogger::UpdateMessage() {
	const std::lock_guard<std::mutex> lock( mMessageMutex );
	const std::lock_guard<std::mutex> lock2( mStateValuesMutex );

	for ( int i = 0; i < mSharedStates.size(); i++ ) {
		//
		// Update the message for the server with the states that have changed locally
		//

		StateRef s = State( mSharedStates[ i ] );
		uint64_t val = s;
		if ( val != mStateValues[ i ] ) {
			bciout << mSharedStates[ i ] << " locally is  " << val;

			std::string message( mSharedStates[ i ] ); // Add name of state
			message.push_back( '\0' ); // Followed by a NULL character for termination

			message.push_back( ( char ) s->Length() / 8 ); // Add size in bytes
			message += std::string( ( char* )( &val ), s->Length() / 8 ); // Add value

			mMessage += message; // Add this segment of the message to the master message

			mStateValues[ i ] = val;
			bciout << "Updated Message";
			bciout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();
		}
	}
}



void HyperscanningNetworkLogger::StopRun() {
	if (!mLogNetwork) return;
	bciwarn << "Stop Run";
	TerminateAndWait();
}



void HyperscanningNetworkLogger::Halt() {
	if (!mLogNetwork) return;
	TerminateAndWait();
}



//
// Set up the connection with the server and set initial values
//

void HyperscanningNetworkLogger::Setup() {

	//
	// Avoid Connecting Twice
	//

	if ( mSocket.IsOpen() ) return;

	//
	// Establish connection to server
	//

	mAddress = ( std::string )OptionalParameter( "IPAddress" );
	mPort = OptionalParameter( "Port" );

	bciout << "Connecting to " << mAddress << ":" << ( unsigned short )mPort;

	mSocket.Open( mAddress, ( unsigned short )mPort ); // Opening socket

	bciout << "Is open: " << mSocket.IsOpen();
	bciout << "Address: " << mSocket.Address();
	bciout << "Connected: " << mSocket.Connected();

	// Ensure connection
	if ( !mSocket.Connected() ) {
		bciwarn << "Not connected to server. Try again.";
		return;
	} else {
		bciout << "Connected to server.";
	}

	//
	// Download Paramater File
	//

	bciout << "Awaiting server parameters...";

	if ( mSocket.Wait() ) {

		// Get the size of the parameter file from the server
		size_t size = GetServerMessageSize();

		bciwarn << sizeof( size_t );
		bciwarn << "Size: " << size;

		if ( size > 0 ) {
			mBuffer = ( char* )malloc( size );

			// Get the parameter file from the server
			GetServerMessage( mBuffer, size );

			// Save the parameter file
			std::string param_file = std::string( mBuffer, size - 1 );
			std::ofstream outfile ( "../parms/CommunicationTask/HyperScanningParameters.prm" );
			outfile << param_file;

			free( mBuffer );

		}
	}

	bciout << "Recieved server parameters.";

	//
	// Get Client Number
	//

	// Get client number message
	if ( mSocket.Wait() ) {
		mBuffer = ( char* ) calloc( 1025, 1 );
		*mBuffer = 'q';

		if ( ::recv(mSocket.Fd(), mBuffer, 1025, 0 ) < 0 ) // read one packet only
			bciwarn << "reading socket: " << errno;

		char* buffer = mBuffer;
		std::string name( buffer );
		buffer += name.size() + 1;
		char size = *buffer++;

		uint64_t val = 0;
		memcpy( &val, buffer, size );

		bciout << "ClientNumber: " << ( int )val;

		mClientNumber = val;

		free( mBuffer );
	}

}

//
// Run the asynchronous server loop
//

int HyperscanningNetworkLogger::OnExecute() {
	if (mLogNetwork){
		while ( !Terminating() ) {
			//
			// Write Message to Server
			//

			{
				std::lock_guard<std::mutex> messageLock(mMessageMutex);
				std::string name(mMessage.c_str());

				if ( mMessage.size() > 0 ) {

					mMessage.push_back('\0');

					bciwarn << "Writing: " << mMessage;
					bciout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();

					if (mSocket.Write(mMessage.c_str(), mMessage.size()) < 0)
					{
						bciwarn << "Error writing to socket: " << errno;
						return -1;
					}

					mMessage = std::string( "" ); // Reset message
				}
			}

			//
			// Read Message From Server
			//

			size_t size = GetServerMessageSize();

			if ( size > 0 ) {
				bciwarn << "Size: " << size;
				mBuffer = ( char* ) calloc( size + 1, 1 );
				GetServerMessage( mBuffer, size );

				bciwarn << "Message: " << mBuffer;

				Interpret(mBuffer);
				free( mBuffer );
			}

		}
	}
	return 1;
}



//
// Interpret the buffer downloaded from the server
//

void HyperscanningNetworkLogger::Interpret( char* buffer ) {
	while ( *buffer != 0 ) {
		bciwarn << "Buffer: " << buffer;

		// Get State Name and Size
		std::string name( buffer );
		buffer += name.size() + 1;
		char size = *buffer++;

		if ( size == 0 ) continue;

		// Get State Value
		std::string value( buffer, size );
		buffer += size;

		uint64_t val = 0;
		memcpy( &val, value.c_str(), size );

		bciwarn << name << ": " << val;
		bciout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::system_clock::now().time_since_epoch() ).count();

		// Set state to be updated to new value
		mStateValuesMutex.lock();
		auto it = find( mSharedStates.begin(), mSharedStates.end(), name );
		if ( it != mSharedStates.end() ) {
			mStateValues[ it - mSharedStates.begin() ] = val;
			mHasUpdated[ it - mSharedStates.begin() ] = false;
		} else {
			bciwarn << "State is not a part of Hyperscanning Shared States";
		}
		mStateValuesMutex.unlock();

	}
}
