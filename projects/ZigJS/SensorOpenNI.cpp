
#include "SensorOpenNI.h"
#include "logging.h"
#include "boost/bind.hpp"

using namespace boost::assign;

// TODO: MOVE OUT OF HERE
static const char* base64_charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

boost::shared_ptr<std::string> base64_encode(const char * dp, unsigned int size)
{
  boost::shared_ptr<std::string> output = boost::make_shared<std::string>("data:image/bmp;base64,");
  std::string& outdata = *output;
  outdata.reserve((outdata.size()) + ((size * 8) / 6) + 2);
  std::string::size_type remaining = size;

  while (remaining >= 3) {
    outdata.push_back(base64_charset[(dp[0] & 0xfc) >> 2]);
    outdata.push_back(base64_charset[((dp[0] & 0x03) << 4) | ((dp[1] & 0xf0) >> 4)]); 
    outdata.push_back(base64_charset[((dp[1] & 0x0f) << 2) | ((dp[2] & 0xc0) >> 6)]);
    outdata.push_back(base64_charset[(dp[2] & 0x3f)]);
    remaining -= 3; dp += 3;
  }
  
  if (remaining == 2) {
    outdata.push_back(base64_charset[(dp[0] & 0xfc) >> 2]);
    outdata.push_back(base64_charset[((dp[0] & 0x03) << 4) | ((dp[1] & 0xf0) >> 4)]); 
    outdata.push_back(base64_charset[((dp[1] & 0x0f) << 2)]);
    outdata.push_back(base64_charset[64]);
  } else if (remaining == 1) {
    outdata.push_back(base64_charset[(dp[0] & 0xfc) >> 2]);
    outdata.push_back(base64_charset[((dp[0] & 0x03) << 4)]); 
    outdata.push_back(base64_charset[64]);
    outdata.push_back(base64_charset[64]);
  }

  return output;
}


// instead of understanding the format, we'll just replace the data from existing valid BMPs
// ugly as hell, but will work just fine
const unsigned char bitmap_header_qvga[] = {
							 0x42, 0x4D, 0x36, 0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 
							 0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x40, 0x01, 
							 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 
							 0x00, 0x00, 0x00, 0x00, 0x00, 0x84, 0x03, 0x00, 0x00, 0x00, 
							 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
							 0x00, 0x00, 0x00, 0x00
							 };

const unsigned char bitmap_header_vga[] = {
							 0x42, 0x4D, 0x36, 0x10, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 
							 0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x80, 0x02, 
							 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 
							 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0x00, 0x00, 
							 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
							 0x00, 0x00, 0x00, 0x00
							 };
// hack - bitmap headers are both the same length...
const unsigned long bmp_header_size = sizeof(bitmap_header_vga); 


boost::shared_ptr<std::string> bitmap_from_depth(const XnDepthMetaData* depth, const XnSceneMetaData* users)
{
	unsigned long totalLength = bmp_header_size + (depth->pMap->Res.X * depth->pMap->Res.Y * 3);
	bool VGA = depth->pMap->Res.X == 640;

	unsigned char * outputBuffer = new unsigned char[totalLength];
	unsigned char * out = outputBuffer;
	// TODO: Code is ugly, I don't care much
	if (VGA) {
		memcpy(outputBuffer, bitmap_header_vga, sizeof(bitmap_header_vga));
		out += sizeof(bitmap_header_vga);
	} else {
		memcpy(outputBuffer, bitmap_header_qvga, sizeof(bitmap_header_qvga));
		out += sizeof(bitmap_header_qvga);
	}

	// TODO: unused?
	//const XnDepthPixel maxDepth = SensorOpenNI::m_depth.GetDeviceMaxDepth();

	// simple copy loop - bitmap has its rows upside down, so we have to invert the rows
	// invert rows copy loop
	for(long y = depth->pMap->Res.Y - 1; y >= 0 ; y--) {

		const XnDepthPixel * in = depth->pData + depth->pMap->Res.X*y;
		// assume labelmap is the same resolution as the depth map
		const XnLabel * inUsers = users->pData + users->pMap->Res.X*y;

		for(long x = 0;
			x < depth->pMap->Res.X;
			++x, out += 3, ++in, ++inUsers) {
				XnDepthPixel pix = *in;
				out[0] = (unsigned char)(*inUsers); // assuming we'll never pass 256 in the label
				out[1] = pix >> 8;
				out[2] = pix & 0xff;
			}
	}
	boost::shared_ptr<std::string> final = base64_encode((const char *)outputBuffer, totalLength);
	delete[] outputBuffer;
	return final;
}

// END HUGE TODO


const int MAX_USERS = 16;

FB::VariantList SensorOpenNI::PositionToVariant(XnPoint3D pos)
{
	FB::VariantList xyz;
	xyz += pos.X, pos.Y, pos.Z;
	return xyz;
}

FB::VariantList SensorOpenNI::OrientationToVariant(XnMatrix3X3 ori)
{
	FB::VariantList result;
	result.push_back(ori.elements[0]);
	result.push_back(ori.elements[3]);
	result.push_back(ori.elements[6]);
	result.push_back(ori.elements[1]);
	result.push_back(ori.elements[4]);
	result.push_back(ori.elements[7]);
	result.push_back(ori.elements[2]);
	result.push_back(ori.elements[5]);
	result.push_back(ori.elements[8]);
	//result.assign(ori.elements, ori.elements + 9);
	return result;
}

FB::VariantList SensorOpenNI::GetJointsList(XnUserID userid)
{
	FB::VariantList result;
	XnSkeletonJointTransformation jointData;

	// quick out if not tracking
	if (!xnIsSkeletonTracking(m_users, userid)) {
		return result;
	}

	// head is the first, right foot the last. probably not the best way.
	for (int i=XN_SKEL_HEAD; i<= XN_SKEL_RIGHT_FOOT; i++) {
		if (xnIsJointAvailable(m_users, (XnSkeletonJoint)i)) {
			xnGetSkeletonJoint(m_users, userid, (XnSkeletonJoint)i, &jointData);
			FB::VariantMap joint;
			joint["id"] = i;
			joint["position"] = PositionToVariant(jointData.position.position);
			joint["rotation"] = OrientationToVariant(jointData.orientation.orientation);
			joint["positionconfidence"] = jointData.position.fConfidence;
			joint["rotationconfidence"] = jointData.orientation.fConfidence;
			result.push_back(joint);
		}
	}

	return result;
}

FB::VariantList SensorOpenNI::MakeUsersList()
{
	// get the users
	XnUInt16 nUsers = MAX_USERS;
	XnUserID aUsers[MAX_USERS];
	xnGetUsers(m_users, aUsers, &nUsers);

	// construct JS object
	FB::VariantList jsUsers;
	XnPoint3D pos;
	for (int i = 0; i < nUsers; i++) {
		xnGetUserCoM(m_users, aUsers[i], &pos);
		FB::VariantMap user;
		user["tracked"] = xnIsSkeletonTracking(m_users, aUsers[i]);
		user["centerofmass"] = PositionToVariant(pos);
		user["id"] = aUsers[i];
		user["joints"] = GetJointsList(aUsers[i]);
		jsUsers += user;
	}

	return jsUsers;
}

FB::VariantList SensorOpenNI::MakeHandsList()
{
	FB::VariantList jsHands;

	for(std::list<HandPoint>::iterator i = m_handpoints.begin(); i != m_handpoints.end(); i++) {
		FB::VariantMap hand;
		hand["id"] = i->handid;
		hand["userid"] = i->userid;
		hand["position"] = PositionToVariant(i->position);
		jsHands += hand;
	}

	return jsHands;
}

//////////JSON
Json::Value SensorOpenNI::PositionToValue(XnPoint3D pos)
{
	Json::Value xyz(Json::arrayValue);
	xyz.resize(3);
	xyz[0u] = pos.X;
	xyz[1u] = pos.Y;
	xyz[2u] = pos.Z;
	return xyz;
}

Json::Value SensorOpenNI::OrientationToValue(XnMatrix3X3 ori)
{
	Json::Value result(Json::arrayValue);
	result.resize(9);
	result[0u] = ori.elements[0];
	result[1u] = ori.elements[3];
	result[2u] = ori.elements[6];
	result[3u] = ori.elements[1];
	result[4u] = ori.elements[4];
	result[5u] = ori.elements[7];
	result[6u] = ori.elements[2];
	result[7u] = ori.elements[5];
	result[8u] = ori.elements[8];
	//result.assign(ori.elements, ori.elements + 9);
	return result;
}

Json::Value SensorOpenNI::GetJointsJsonList(XnUserID userid)
{
	Json::Value result(Json::arrayValue);

	// quick out if not tracking
	if (!xnIsSkeletonTracking(m_users, userid)) {
		return result;
	}

	int jointCount = 0;
	for (int i=XN_SKEL_HEAD; i<= XN_SKEL_RIGHT_FOOT; i++) {
		if (xnIsJointAvailable(m_users, (XnSkeletonJoint)i)) {
			jointCount++;
		}
	}
	result.resize(jointCount);
	XnSkeletonJointTransformation jointData;
	int position = 0;
	// head is the first, right foot the last. probably not the best way.
	for (int i=XN_SKEL_HEAD; i<= XN_SKEL_RIGHT_FOOT; i++) {
		if (xnIsJointAvailable(m_users, (XnSkeletonJoint)i)) {
			xnGetSkeletonJoint(m_users, userid, (XnSkeletonJoint)i, &jointData);
			Json::Value joint(Json::objectValue);
			joint["id"] = i;
			joint["position"] = PositionToValue(jointData.position.position);
			joint["rotation"] = OrientationToValue(jointData.orientation.orientation);
			joint["positionconfidence"] = jointData.position.fConfidence;
			joint["rotationconfidence"] = jointData.orientation.fConfidence;
			result[position] = joint;
			position++;
		}
	}

	return result;
}

Json::Value SensorOpenNI::MakeUsersJsonList()
{
	// get the users
	XnUInt16 nUsers = MAX_USERS;
	XnUserID aUsers[MAX_USERS];
	xnGetUsers(m_users, aUsers, &nUsers);

	// construct JS object
	Json::Value jsUsers(Json::arrayValue);
	jsUsers.resize(nUsers);
	XnPoint3D pos;
	for (int i = 0; i < nUsers; i++) {
		xnGetUserCoM(m_users, aUsers[i], &pos);
		Json::Value user(Json::objectValue);
		user["tracked"] = xnIsSkeletonTracking(m_users, aUsers[i]);
		user["centerofmass"] = PositionToValue(pos);
		user["id"] = aUsers[i];
		user["joints"] = GetJointsJsonList(aUsers[i]);
		jsUsers[i] = user;
	}

	return jsUsers;
}

Json::Value SensorOpenNI::MakeHandsJsonList()
{
	Json::Value jsHands(Json::arrayValue);
	jsHands.resize(m_handpoints.size());
	int idx = 0;
	for(std::list<HandPoint>::iterator i = m_handpoints.begin(); i != m_handpoints.end(); i++) {
		Json::Value hand(Json::objectValue);
		hand["id"] = i->handid;
		hand["userid"] = i->userid;
		hand["position"] = PositionToValue(i->position);
		hand["focusposition"] = PositionToValue(i->focusposition);
		jsHands[idx] = hand;
		idx++;
	}

	return jsHands;
}

XnUserID SensorOpenNI::WhichUserDoesThisPointBelongTo(XnPoint3D point)
{
	XnPoint3D proj;
	xnConvertRealWorldToProjective(m_depth, 1, &point, &proj);
	xnGetUserPixels(m_users, 0, m_pSceneMD);
	return m_pSceneMD->pData[(XnUInt32)proj.X + (XnUInt32)proj.Y * m_pSceneMD->pMap->Res.X];
}

void XN_CALLBACK_TYPE SensorOpenNI::GestureRecognizedHandler(XnNodeHandle generator, const XnChar* strGesture, const XnPoint3D* pIDPosition, const XnPoint3D* pEndPosition, void* pCookie)
{
	SensorOpenNI * instance = reinterpret_cast<SensorOpenNI *>(pCookie);
	if (instance) {
		instance->GestureRecognizedHandlerImpl(strGesture, pIDPosition, pEndPosition);
	}
}
void SensorOpenNI::GestureRecognizedHandlerImpl(const XnChar* strGesture, const XnPoint3D* pIDPosition, const XnPoint3D* pEndPosition)
{
	xnStartTracking(m_hands, pEndPosition);
}

void XN_CALLBACK_TYPE SensorOpenNI::HandCreateHandler(XnNodeHandle generator, XnUserID user, const XnPoint3D* pPosition, XnFloat fTime, void* pCookie)
{
	SensorOpenNI * instance = reinterpret_cast<SensorOpenNI *>(pCookie);
	if (instance) {
		instance->HandCreateHandlerImpl(user, pPosition, fTime);
	}
}
void SensorOpenNI::HandCreateHandlerImpl(XnUserID user, const XnPoint3D* pPosition, XnFloat fTime)
{
	m_handpoints.push_back(HandPoint(user, WhichUserDoesThisPointBelongTo(*pPosition), *pPosition, *pPosition));
}

void XN_CALLBACK_TYPE SensorOpenNI::HandUpdateHandler(XnNodeHandle generator, XnUserID user, const XnPoint3D* pPosition, XnFloat fTime, void* pCookie)
{
	SensorOpenNI * instance = reinterpret_cast<SensorOpenNI *>(pCookie);
	if (instance) {
		instance->HandUpdateHandlerImpl(user, pPosition, fTime);
	}
}

void SensorOpenNI::HandUpdateHandlerImpl(XnUserID user, const XnPoint3D* pPosition, XnFloat fTime)
{
	// NOTE: user is actually handid - OpenNI ftl
	for(std::list<HandPoint>::iterator i = m_handpoints.begin(); i != m_handpoints.end(); i++) {
		if (i->handid == user) {
			i->position.X = pPosition->X;
			i->position.Y = pPosition->Y;
			i->position.Z = pPosition->Z;
			// TODO: possibly check for user id again in case its 0
			break;
		}
	}

}

void XN_CALLBACK_TYPE SensorOpenNI::HandDestroyHandler(XnNodeHandle generator, XnUserID user, XnFloat fTime, void* pCookie)
{
	SensorOpenNI * instance = reinterpret_cast<SensorOpenNI *>(pCookie);
	if (instance) {
		instance->HandDestroyHandlerImpl(user, fTime);
	}
}
void SensorOpenNI::HandDestroyHandlerImpl(XnUserID user, XnFloat fTime)
{
	for(std::list<HandPoint>::iterator i = m_handpoints.begin(); i != m_handpoints.end(); ) {
		if (i->handid == user) {
			m_handpoints.erase(i);
			break;
		} else {
			++i;
		}
	}
}

void XN_CALLBACK_TYPE SensorOpenNI::OnNewUser(XnNodeHandle generator, const XnUserID nUserId, void* pCookie)
{
	SensorOpenNI * instance = reinterpret_cast<SensorOpenNI *>(pCookie);
	if (instance) {
		instance->OnNewUserImpl(nUserId);
	}
}
void SensorOpenNI::OnNewUserImpl(const XnUserID nUserId)
{
	if (xnNeedPoseForSkeletonCalibration(m_users)) {
		//std::string
		xnStartPoseDetection(m_users, "Psi", nUserId);
	} else {
		xnStartSkeletonTracking(m_users, nUserId);
	}
}

void XN_CALLBACK_TYPE SensorOpenNI::OnLostUser(XnNodeHandle generator, const XnUserID nUserId, void* pCookie)
{
	SensorOpenNI * instance = reinterpret_cast<SensorOpenNI *>(pCookie);
	if (instance) {
		instance->OnLostUserImpl(nUserId);
	}
}
void SensorOpenNI::OnLostUserImpl(const XnUserID nUserId)
{
}


void XN_CALLBACK_TYPE SensorOpenNI::OnPoseDetected(XnNodeHandle poseDetection, const XnChar* strPose, XnUserID nId, void* pCookie)
{
	SensorOpenNI * instance = reinterpret_cast<SensorOpenNI *>(pCookie);
	if (instance) {
		instance->OnPoseDetectedImpl(poseDetection, strPose, nId);
	}
}
void SensorOpenNI::OnPoseDetectedImpl(XnNodeHandle poseDetection, const XnChar* strPose, XnUserID nId)
{
	// Stop detecting the pose
	xnStopPoseDetection(poseDetection, nId);

	// Start calibrating
	xnRequestSkeletonCalibration(m_users, nId, TRUE);
}

void XN_CALLBACK_TYPE SensorOpenNI::OnCalibrationStart(XnNodeHandle skeleton, const XnUserID nUserId, void* pCookie)
{
	SensorOpenNI * instance = reinterpret_cast<SensorOpenNI *>(pCookie);
	if (instance) {
		instance->OnCalibrationStartImpl(nUserId);
	}
}
void SensorOpenNI::OnCalibrationStartImpl(const XnUserID nUserId)
{
}

void XN_CALLBACK_TYPE SensorOpenNI::OnCalibrationEnd(XnNodeHandle skeleton, const XnUserID nUserId, XnBool bSuccess, void* pCookie)
{
	SensorOpenNI * instance = reinterpret_cast<SensorOpenNI *>(pCookie);
	if (instance) {
		instance->OnCalibrationEndImpl(skeleton, nUserId, bSuccess);
	}
}
void SensorOpenNI::OnCalibrationEndImpl(XnNodeHandle skeleton, const XnUserID nUserId, XnBool bSuccess)
{
	// If this was a successful calibration
	if (bSuccess) {
		// start tracking
		xnStartSkeletonTracking(skeleton, nUserId);
	} else {
		// Restart pose detection
		xnStartPoseDetection(m_users, "Psi", nUserId);
	}	
}
void XN_CALLBACK_TYPE SensorOpenNI::ErrorCallback(XnStatus errorState, void *pCookie) {
	FBLOG_INFO("errorShit", "global error!");
	if (XN_STATUS_OK != errorState) {
		reinterpret_cast<SensorOpenNI *>(pCookie)->m_error = true;
	}
}

SensorOpenNI::SensorOpenNI() : 
	m_initialized(false), m_error(false),
	m_lastNewDataTime(0xFFFFFFFFFFFFFFFFULL),
	m_pContext(NULL),
	m_pSceneMD(NULL), m_pDepthMD(NULL),
	m_depth(NULL), m_users(NULL), m_device(NULL),
	m_gestures(NULL), m_hands(NULL)
{
	
	XnStatus nRetVal = XN_STATUS_OK;
	nRetVal = xnInit(&m_pContext);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_INFO("xnInit", "fail context init");
		return;
	} else {
		FBLOG_INFO("xnInit", "ok context init");
	}
	
	nRetVal = xnRegisterToGlobalErrorStateChange(m_pContext, &SensorOpenNI::ErrorCallback, this, &m_errorCB);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_INFO("xnInit", "fail register for error callback");
		xnContextRelease(m_pContext);
		m_pContext = NULL;
		return;
	} else {
		FBLOG_INFO("xnInit", "ok register for error callback");
	}
	m_pSceneMD = xnAllocateSceneMetaData();
	m_pDepthMD = xnAllocateDepthMetaData();
	xnOSStrCopy(m_license.strKey, "0KOIk2JeIBYClPWVnMoRKn5cdY4=", sizeof(m_license.strKey));
	xnOSStrCopy(m_license.strVendor, "PrimeSense", sizeof(m_license.strVendor));
	xnAddLicense(m_pContext, &m_license);

	nRetVal = xnCreateAnyProductionTree(m_pContext, XN_NODE_TYPE_DEVICE, NULL, &m_device, NULL);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail get device");
		m_lastFrame = -6;
		xnContextRelease(m_pContext);
		m_pContext = NULL;
		return;
	} else {
		FBLOG_DEBUG("xnInit", "ok get device");
	}
	nRetVal = xnCreateAnyProductionTree(m_pContext, XN_NODE_TYPE_DEPTH, NULL, &m_depth, NULL);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail get depth");
		m_lastFrame = -6;
		xnContextRelease(m_pContext);
		m_pContext = NULL;
		return;
	} else {
		FBLOG_DEBUG("xnInit", "ok get depth");
	}
	
	nRetVal = xnCreateAnyProductionTree(m_pContext, XN_NODE_TYPE_GESTURE, NULL, &m_gestures, NULL);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail get gesture");
		m_lastFrame = -6;
		xnContextRelease(m_pContext);
		m_pContext = NULL;
		return;
	} else {
		FBLOG_INFO("xnInit", "ok get gesture");
	}
	
	nRetVal = xnCreateAnyProductionTree(m_pContext, XN_NODE_TYPE_HANDS, NULL, &m_hands, NULL);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail get hands");
		m_lastFrame = -6;
		xnContextRelease(m_pContext);
		return;
	} else {
		FBLOG_INFO("xnInit", "ok get hands");
	}

	nRetVal = xnCreateAnyProductionTree(m_pContext, XN_NODE_TYPE_USER, NULL, &m_users, NULL);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail create production tree");
		m_lastFrame = -6;
		xnContextRelease(m_pContext);
		m_pContext = NULL;
		return;
	} else {
		FBLOG_INFO("xnInit", "ok create production tree");
	}

	// make sure global mirror is on
	xnSetGlobalMirror(m_pContext, true);

	XnCallbackHandle ignore;
	// register to gesture/hands callbacks
	xnRegisterGestureCallbacks(m_gestures, &SensorOpenNI::GestureRecognizedHandler, NULL, this, &ignore);
	xnRegisterHandCallbacks(m_hands, &SensorOpenNI::HandCreateHandler, 
								  &SensorOpenNI::HandUpdateHandler, 
								  &SensorOpenNI::HandDestroyHandler,
								  this, &ignore);

	xnRegisterUserCallbacks(m_users, &SensorOpenNI::OnNewUser, 
								  &SensorOpenNI::OnLostUser, 
								  this, &ignore);
	xnSetSkeletonProfile(m_users, XN_SKEL_PROFILE_ALL);
	xnRegisterCalibrationCallbacks(m_users, &SensorOpenNI::OnCalibrationStart,
														  &SensorOpenNI::OnCalibrationEnd,
														  this, &ignore);
	xnSetSkeletonSmoothing(m_users, 0.5);
	xnRegisterToPoseCallbacks(m_users, &SensorOpenNI::OnPoseDetected, NULL, this, &ignore);

	xnAddGesture (m_gestures, "Wave",  NULL); //no bounding box
	xnAddGesture (m_gestures, "Click",  NULL); //no bounding box

	nRetVal = xnStartGeneratingAll(m_pContext);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_INFO("xnInit", "fail start generating");
		m_lastFrame = -1;
		xnContextRelease(m_pContext);
		m_pContext = NULL;
		return;
	} else {
		FBLOG_INFO("xnInit", "ok start generating");
	}
	nRetVal = xnWaitAndUpdateAll(m_pContext); // do a single real read
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_INFO("xnInit", "fail read frame");
		m_lastFrame = -1;
		xnContextRelease(m_pContext);
		m_pContext = NULL;
		return;
	} else {
		FBLOG_INFO("xnInit", "ok read frame");
	}

	m_initialized = true;
}

SensorOpenNI::~SensorOpenNI()
{
	if (NULL != m_pSceneMD) {
		xnFreeSceneMetaData(m_pSceneMD);
		m_pSceneMD = NULL;
	}
	if (NULL != m_pDepthMD) {
		xnFreeDepthMetaData(m_pDepthMD);
		m_pDepthMD = NULL;
	}
	if (NULL != m_hands) { 
		xnProductionNodeRelease(m_hands);
		m_hands = NULL;
	}
	if (NULL != m_users) { 
		xnProductionNodeRelease(m_users);
		m_hands = NULL;
	}
	if (NULL != m_gestures) { 
		xnProductionNodeRelease(m_gestures);
		m_gestures = NULL;
	}
	if (NULL != m_depth) { 
		xnProductionNodeRelease(m_depth);
		m_depth = NULL;
	}
	if (NULL != m_device) { 
		xnProductionNodeRelease(m_device);
		m_device = NULL;
	}
	if (NULL != m_pContext) {
		xnContextRelease(m_pContext);
		m_pContext = NULL;
	}
}
bool SensorOpenNI::Valid() const {
	return m_initialized && (!m_error);
}

bool SensorOpenNI::ReadFrame() {
	//TODO: refactor so that the per-frame results are encapsulated in an object
	//      instead of having a stateful object

	if (!Valid()) return false;
	m_gotImage = false;

	XnStatus nRetVal = xnWaitNoneUpdateAll(m_pContext);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_INFO("ReadFrame", "fail wait & update");
		m_initialized = false; // can't read no depth no more
		return false; //TODO: throw exception (so we know to create a new reader thingy)
	}
	xnGetDepthMetaData(m_depth, m_pDepthMD);

	if (m_lastFrame == (int)m_pDepthMD->pMap->pOutput->nFrameID) {
		if (m_lastNewDataTime == 0xFFFFFFFFFFFFFFFFULL) {
			xnOSGetTimeStamp(&m_lastNewDataTime);
		}
		XnUInt64 currentTime;
		xnOSGetTimeStamp(&currentTime);
		if ((currentTime - m_lastNewDataTime) > 5*1000ULL) {
			FBLOG_INFO("ReadFrame", "timed out waiting for new data!");
			m_error = true;
		}
		return false; // not a new frame, do nothing
	}
	
	m_lastFrame = (int)m_pDepthMD->pMap->pOutput->nFrameID;
	xnOSGetTimeStamp(&m_lastNewDataTime);
	//m_lastNewDataTime = m_depthMD.Timestamp();
	xnGetUserPixels(m_users, 0, m_pSceneMD);

	Json::Value pluginData;
	pluginData["hands"] = MakeHandsJsonList();
	pluginData["users"] = MakeUsersJsonList();
	pluginData["frameId"] = m_lastFrame;
	m_eventData = m_writer.write(pluginData); //TODO: unhack
	return true;
}

boost::shared_ptr< FB::variant > SensorOpenNI::GetImageBase64() const {
	if (!Valid()) return boost::shared_ptr< FB::variant >();
	if (!m_gotImage) {
		m_imageData = boost::make_shared< FB::variant >(*bitmap_from_depth(m_pDepthMD, m_pSceneMD));
		m_gotImage = true;
	}
	return m_imageData;
}

const std::string& SensorOpenNI::GetEventData() const {
	return m_eventData;
}