#pragma once

#include <cstring>
#include <cstdint>

#include "ptrlist.h"

#include "IPlugPlatform.h"
#include "IPlugConstants.h"
#include "IPlugStructs.h"
#include "IPlugUtilities.h"
#include "NChanDelay.h"

/**
 * @file
 * @copydoc IPlugProcessor
*/

struct IPlugConfig;

/** The base class for IPlug Audio Processing. It knows nothing about presets or parameters or user interface.  */

template<typename sampleType>
class IPlugProcessor
{
public:
  IPlugProcessor(IPlugConfig config, EAPI plugAPI);
  virtual ~IPlugProcessor();

#pragma mark - Methods you implement in your plug-in class - you do not call these methods

  /** Override in your plug-in class to process audio
   * In ProcessBlock you are always guaranteed to get valid pointers to all the channels the plugin requested (the maximum possible input channel count and the maximum possible output channel count including multiple buses). If the host hasn't connected all the pins,
   * the unconnected channels will be full of zeros.
   * THIS METHOD IS CALLED BY THE HIGH PRIORITY AUDIO THREAD - You should be careful not to do any unbounded, blocking operations such as file I/O which could cause audio dropouts
   * @param inputs Two-dimensional array containing the non-interleaved input buffers of audio samples for all channels
   * @param outputs Two-dimensional array for audio output (non-interleaved).
   * @param nFrames The block size for this block: number of samples per channel.*/
  virtual void ProcessBlock(sampleType** inputs, sampleType** outputs, int nFrames);

  /** Override this method which is called prior to ProcessBlock(), to handle incoming MIDI messages.
   * You can use IMidiQueue in combination with this method in order to queue the message and process at the appropriate time in ProcessBlock()
   * THIS METHOD IS CALLED BY THE HIGH PRIORITY AUDIO THREAD - You should be careful not to do any unbounded, blocking operations such as file I/O which could cause audio dropouts
   * @param msg The incoming midi message (includes a timestamp to indicate the offset in the forthcoming block of audio to be processed in ProcessBlock()) */
  virtual void ProcessMidiMsg(const IMidiMsg& msg);
  
  /** Override this method which is calledThis method is called prior to ProcessBlock(), to handle incoming MIDI System Exclusive (SysEx) messages.
   * THIS METHOD IS CALLED BY THE HIGH PRIORITY AUDIO THREAD - You should be careful not to do any unbounded, blocking operations such as file I/O which could cause audio dropouts */
  virtual void ProcessSysEx(ISysEx& msg) {}

  /** Override this method in your plug-in class to do something prior to playback etc. (e.g.clear buffers, update internal DSP with the latest sample rate) */
  virtual void OnReset() { TRACE; }
  
  /** Override OnActivate() which should be called by the API class when a plug-in is "switched on" by the host on a track when the channel count is known.
   * This may not work reliably because different hosts have different interpretations of "activate".
   * Unlike OnReset() which called when the transport is reset or the sample rate changes OnActivate() is a good place to handle change of I/O connections.
   * @param active \c True if the host has activated the plug-in */
  virtual void OnActivate(bool active) { TRACE; }
  
  /** Override this method to get an "idle"" call from the audio processing thread in VST2 plug-ins.
   * THIS METHOD IS CALLED BY THE HIGH PRIORITY AUDIO THREAD - You should be careful not to do any unbounded, blocking operations such as file I/O which could cause audio dropouts
   * Only active if USE_IDLE_CALLS preprocessor macro is defined */
  virtual void OnIdle() {}
  
#pragma mark - Methods you can call - some of which have custom implementations in the API classes, some implemented in IPlugProcessor.cpp
  
  /** Send a single MIDI message
   * @param msg The IMidiMsg to send
   * @return /c true if successful */
  virtual bool SendMidiMsg(const IMidiMsg& msg) = 0;
  
  /** Send a collection of MIDI messages
   * @param msg The IMidiMsg to send
   * @return /c true if successful */
  bool SendMidiMsgs(WDL_TypedBuf<IMidiMsg>& msgs);
  
  /** Send a single MIDI System Exclusive (SysEx) message
   * @param msg The ISysEx to send
   * @return /c true if successful */
  virtual bool SendSysEx(ISysEx& msg) { return false; }
  
  /** @return Sample rate (in Hz) */
  double GetSampleRate() const { return mSampleRate; }
  
  /** @return Current block size in samples */
  int GetBlockSize() const { return mBlockSize; }
  
  /** @return Plugin latency (in samples) */
  int GetLatency() const { return mLatency; }
  
  /** @return The tail size in samples (useful for reverberation plug-ins, that may need to decay after the transport stops or an audio item ends) */
  int GetTailSize() { return mTailSize; }
  
  /** @return \c True if the plugin is currently bypassed */
  bool GetBypassed() const { return mBypassed; }
  
  /** @return \c True if the plugin is currently rendering off-line */
  bool GetRenderingOffline() const { return mRenderingOffline; };
  
#pragma mark -
  /** @return The number of samples elapsed since start of project timeline. */
  int GetSamplePos() const { return mTimeInfo.mSamplePos; }
  
  /** @return The Tempo in beats per minute */
  double GetTempo() const { return mTimeInfo.mTempo; }
  
  /** @return The Tempo in beats per minute */
  double GetSamplesPerBeat() const;
  
  /** @param numerator The upper part of the current time signature e.g "6" in the time signature 6/8
   *  @param denominator The lower part of the current time signature e.g "8" in the time signature 6/8 */
  void GetTimeSig(int& numerator, int& denominator) { numerator = mTimeInfo.mNumerator; denominator = mTimeInfo.mDenominator; }
  
#pragma mark -
  /** Used to determine the maximum number of input or output buses based on what was specified in the channel I/O config string
   * @param direction Return input or output bus count
   * @return The maximum bus count across all channel I/O configs */
  int MaxNBuses(ERoute direction) const { if(direction == kInput) { return mMaxNInBuses; } else { return mMaxNOutBuses; } }
  
  /** For a given input or output bus what is the maximum possible number of channels
   * @param direction Return input or output bus count
   * @param busIdx The index of the bus to look up
   * @return return The maximum number of channels on that bus */
  int MaxNChannelsForBus(ERoute direction, int busIdx) const;
  
  /** Check if we have any wildcard characters in the channel io configs
   * @param direction Return input or output bus count
   * @return /true if the bus has a wildcard, meaning it should work on any number of channels */
  bool HasWildcardBus(ERoute direction) const { return mIOConfigs.Get(0)->ContainsWildcard(direction); } // /todo only supports a single I/O config
  
  /** @return The number of channel I/O configs derived from the channel io string*/
  int NIOConfigs() const { return mIOConfigs.GetSize(); }
  
  /** @return Pointer to an IOConfig at idx. Can return nullptr if idx is invalid */
  IOConfig* GetIOConfig(int idx) { return mIOConfigs.Get(idx); }
  
  /** @return Total number of input channel buffers */
  int NInChannels() const { return mInChannels.GetSize(); }
  
  /** @return Total number of output channel buffers */
  int NOutChannels() const { return mOutChannels.GetSize(); }
  
  /** @param chIdx channel index
    * @return /c true if the host has connected this input channel*/
  bool IsInChannelConnected(int chIdx) const;
  
  /** @param chIdx channel index
   * @return /c true if the host has connected this output channel*/
  bool IsOutChannelConnected(int chIdx) const;
  
  /** @return The number of input channels connected. WARNING: this assumes consecutive channel connections*/
  int NInChansConnected() const;
  
  /** @return The number of output channels connected. WARNING: this assumes consecutive channel connections*/
  int NOutChansConnected() const;
  
  /** Check if a certain configuration of input channels and output channels is allowed based on the channel I/O configs
   * @param NInputChans Number of inputs to test, if set to -1 = check NOutputChans only
   * @param NOutputChans Number of outputs to test, if set to -1 = check NInputChans only
   * @return /c true if the configurations is valid */
  bool LegalIO(int NInputChans, int NOutputChans) const; //TODO: this should be updated
  
  /** @return c/ true if this plug-in has a side-chain input, which may not necessarily be active in the current io config */
  bool HasSidechainInput() const { return mMaxNInBuses > 1; }
  
  /** @return The number of channels and the side-chain input /todo this will change */
  int NSidechainChannels() const { return 1; } // TODO: this needs to be more flexible, based on channel I/O
  
  /** This is called by IPlugVST in order to limit a plug-in to stereo I/O for certain picky hosts /todo may no longer be relevant*/
  void LimitToStereoIO();//TODO: this should be updated

  /* @return /c true if the plug-in was configured as an instrument at compile time*/
  bool IsInstrument() const { return mIsInstrument; }
  
  /* @return /c true if the plug-in was configured to receive midi at compile time*/
  bool DoesMIDI() const { return mDoesMIDI; }
  
  /**  This allows you to label input channels in supporting VST2 hosts.
   * For example a 4 channel plug-in that deals with FuMa bformat first order ambisonic material might label these channels "W", "X", "Y", "Z", rather than the default "input 1", "input 2", "input 3", "input 4"
   * @param idx The index of the channel that you wish to label
   * @param label The label for the channel*/
  void SetInputLabel(int idx, const char* label);
  
  /**  This allows you to label  output channels in supporting VST2 hosts.
   * For example a 4 channel plug-in that deals with FuMa bformat first order ambisonic material might label these channels "W", "X", "Y", "Z", rather than the default " output 1", " output 2", " output 3", " output 4"
   * @param idx The index of the channel that you wish to label
   * @param label The label for the channel*/
  void SetOutputLabel(int idx, const char* label);
  
  /** Call this if the latency of your plug-in changes after initialization (perhaps from OnReset() )
   * This may not be supported by the host. The method is virtual because it's overridden in API classes.
   @param samples Latency in samples */
  virtual void SetLatency(int latency);

  /** Call this method if you need to update the tail size at runtime, for example if the decay time of your reverb effect changes
   * Some apis have special interpretations of certain numbers. For VST3 set to 0xffffffff for infinite tail, or 0 for none (default)
   * For VST2 setting to 1 means no tail
   * @param tailSizeSamples the new tailsize in samples*/
  void SetTailSize(int tailSize) { mTailSize = tailSize; }
  
  /** A static method to parse the config.h channel I/O string.
   * @param IOStr Space separated cstring list of I/O configurations for this plug-in in the format ninchans-noutchans. A hypen character \c(-) deliminates input-output. Supports multiple buses, which are indicated using a period \c(.) character. For instance plug-in that supports mono input and mono output with a mono side-chain input could have a channel io string of "1.1-1". A drum synthesiser with four stereo output busses could be configured with a io string of "0-2.2.2.2";
   * @param channelIOList A list of pointers to ChannelIO structs, where we will store here
   * @param totalNInChans The total number of input channels across all buses will be stored here
   * @param totalNOutChans The total number of output channels across all buses will be stored here
   * @param totalNInBuses The total number of input buses across all channel io configs will be stored here
   * @param totalNOutBuses The total number of output buses across all channel io configs will be stored here
   * @return The number of space separated channel io configs that have been detected in IOStr */
  static int ParseChannelIOStr(const char* IOStr, WDL_PtrList<IOConfig>& channelIOList, int& totalNInChans, int& totalNOutChans, int& totalNInBuses, int& totalNOutBuses);
  
protected:
#pragma mark - Methods called by the API class - you do not call these methods in your plug-in class
  void SetInputChannelConnections(int idx, int n, bool connected);
  void SetOutputChannelConnections(int idx, int n, bool connected);
  void AttachInputBuffers(int idx, int n, PLUG_SAMPLE_DST** ppData, int nFrames);
  void AttachInputBuffers(int idx, int n, PLUG_SAMPLE_SRC** ppData, int nFrames);
  void AttachOutputBuffers(int idx, int n, PLUG_SAMPLE_DST** ppData);
  void AttachOutputBuffers(int idx, int n, PLUG_SAMPLE_SRC** ppData);
  void PassThroughBuffers(PLUG_SAMPLE_SRC type, int nFrames);
  void PassThroughBuffers(PLUG_SAMPLE_DST type, int nFrames);
  void ProcessBuffers(PLUG_SAMPLE_SRC type, int nFrames);
  void ProcessBuffers(PLUG_SAMPLE_DST type, int nFrames);
  void ProcessBuffersAccumulating(PLUG_SAMPLE_SRC type, int nFrames);
  void ZeroScratchBuffers();
  void SetSampleRate(double sampleRate) { mSampleRate = sampleRate; }
  void SetBypassed(bool bypassed) { mBypassed = bypassed; }
  void SetBlockSize(int blockSize);
  void SetTimeInfo(const ITimeInfo& timeInfo) { mTimeInfo = timeInfo; }
  void SetRenderingOffline(bool renderingOffline) { mRenderingOffline = renderingOffline; }
  const WDL_String& GetInputLabel(int idx) { return mInChannels.Get(idx)->mLabel; }
  const WDL_String& GetOutputLabel(int idx) { return mOutChannels.Get(idx)->mLabel; }
private:
  /** \c True if the plug-in is an instrument */
  bool mIsInstrument;
  /** \c True if the plug-in accepts MIDI input */
  bool mDoesMIDI;
  /** Plug-in latency (in samples) */
  int mLatency;
  /** Current sample rate (in Hz) */
  double mSampleRate = DEFAULT_SAMPLE_RATE;
  /** Current block size (in samples) */
  int mBlockSize = 0;
  /** Current tail size (in samples) */
  int mTailSize = 0;
  /** \c True if the plug-in is bypassed */
  bool mBypassed = false;
  /** \c True if the plug-in is rendering off-line*/
  bool mRenderingOffline = false;
  /** The maximum number of input buses detected across all channels I/O configs */
  int mMaxNInBuses;
  /** The maximum number of output buses detected across all channels I/O configs */
  int mMaxNOutBuses;
  /** A list of IOConfig structures populated by ParseChannelIOStr in the IPlugProcessor constructor */
  WDL_PtrList<IOConfig> mIOConfigs;
  /* The data to use as a scratch buffer for audio input */
  WDL_TypedBuf<sampleType*> mInData;
  /* The data to use as a scratch buffer for audio output */
  WDL_TypedBuf<sampleType*> mOutData;
  /* A list of IChannelData structures corresponding to every input channel */
  WDL_PtrList<IChannelData<>> mInChannels;
  /* A list of IChannelData structures corresponding to every output channel */
  WDL_PtrList<IChannelData<>> mOutChannels;
  /** Contains detailed information about the transport state */
  ITimeInfo mTimeInfo;
protected: // these members are protected because they need to be access by the API classes, and don't want a setter/getter
  /** Pointer to a multichannel delay line used to delay the bypassed signal when a plug-in with latency is bypassed. */
  NChanDelayLine<sampleType>* mLatencyDelay = nullptr;
};

#include "IPlugProcessor.cpp"