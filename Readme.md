# HyperscanningApplicationBase
Child class of BCI2000 ApplicationBase.

## Documentation

### Methods
`void HyperscanningApplicationBase::Publish() override;`  
`void HyperscanningApplicationBase::AutoConfig( const SignalProperties& Input ) override;`  
`void HyperscanningApplicationBase::Preflight( const SignalProperties& Input, SignalProperties& Output ) const override;`  
`void HyperscanningApplicationBase::Initialize( const SignalProperties& Input, const SignalProperties& Output ) override;`  
`void HyperscanningApplicationBase::StartRun() override;`  
`void HyperscanningApplicationBase::Process( const GenericSignal& Input, GenericSignal& Output ) override;`  
`void HyperscanningApplicationBase::StopRun() override;`  
`void HyperscanningApplicationBase::Halt() override;`  
`void HyperscanningApplicationBase::Resting() override;`  
`
`virtual void HyperscanningApplicationBase::SharedPublish();`  
`virtual void HyperscanningApplicationBase::SharedAutoConfig( const SignalProperties& Input );`  
`virtual void HyperscanningApplicationBase::SharedPreflight( const SignalProperties& Input, SignalProperties& Output ) const;`  
`virtual void HyperscanningApplicationBase::SharedInitialize( const SignalProperties& Input, const SignalProperties& Output );`  
`virtual void HyperscanningApplicationBase::SharedStartRun();`  
`virtual void HyperscanningApplicationBase::SharedProcess( const GenericSignal& Input, GenericSignal& Output );`  
`virtual void HyperscanningApplicationBase::SharedResting();`  
`virtual void HyperscanningApplicationBase::SharedStopRun();`  
`virtual void HyperscanningApplicationBase::SharedHalt();`  

