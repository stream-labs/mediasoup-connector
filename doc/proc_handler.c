// Every function accepts a string and returns a string, json

// Refer to C++ doc for additional information on functions referred to below
// https://mediasoup.org/documentation/v3/libmediasoupclient/api/

func_load_device(string jsonInput)
	// Description: Creates a mediasoupclient::Device and then performs "device.Load(routerRtpCapabilities, peerConnectionOptions = nullptr)"
	//				If one already exists, this function will log an error and do nothing
	//
	// jsonInput["rotuerRtpCapabilities"]
	//
	// output["deviceRtpCapabilities"] = device.GetRtpCapabilities();
	// output["deviceSctpCapabilities"] = device.GetSctpCapabilities();
	// output["version"] = mediasoupclient::Version();
	// output["clientId"] = rtc::CreateRandomId();
	
func_create_send_transport(string jsonInput)
	// Description: Performs "device.CreateSendTransport(listener, id, iceParameters, iceCandidates, dtlsParameters, peerConnectionOptions = nullptr, appData = {})" and saves the object in memory
	//				If one already exists, this function will log an error and do nothing
	//
	// jsonInput["id"]
	// jsonInput["iceParameters"]
	// jsonInput["iceCandidates"] 
	// jsonInput["dtlsParameters"] 
	// jsonInput["iceServers"] (optional)
	//
	// output["senderId"] = SendTransport::GetId()

func_create_receive_transport(string jsonInput)
	// Description: Performs "device.CreateRecvTransport(listener, id, iceParameters, iceCandidates, dtlsParameters, peerConnectionOptions = nullptr, appData = {})" and saves the object in memory
	//				If one already exists, this function will log an error and do nothing
	//
	// jsonInput["id"]
	// jsonInput["iceParameters"]
	// jsonInput["iceCandidates"]
	// jsonInput["dtlsParameters"]
	// jsonInput["sctpParameters"] (optional)
	// jsonInput["iceServers"] (optional)
	//
	// output["receiverId"] = RecvTransport::GetId()

func_video_consumer_response(string jsonInput)

func_audio_consumer_response(string jsonInput)

func_create_audio_producer(string jsonInput)

func_create_video_producer(string jsonInput)

func_produce_result(string jsonInput)

func_connect_result(string jsonInput)

func_stop_receiver(string jsonInput)

func_stop_sender(string jsonInput)

func_stop_consumer(string jsonInput)

func_stop_producer(string jsonInput)