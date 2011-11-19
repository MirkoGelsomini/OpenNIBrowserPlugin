/**********************************************************\

  Auto-generated ZigJS.cpp

  This file contains the auto-generated main plugin object
  implementation for the awesome project

\**********************************************************/

#include "ZigJSAPI.h"

#include "ZigJS.h"

using namespace boost::assign;

xn::Context ZigJS::s_context;
xn::DepthGenerator ZigJS::s_depth;
xn::GestureGenerator ZigJS::s_gestures;
xn::HandsGenerator ZigJS::s_hands;
xn::UserGenerator ZigJS::s_users;

int ZigJS::s_lastFrame;
XN_THREAD_HANDLE ZigJS::s_threadHandle = NULL;
volatile bool ZigJS::s_quit = false;

std::list< ZigJSAPIWeakPtr > ZigJS::s_listeners;
boost::recursive_mutex ZigJS::s_listenersMutex;

void ZigJS::AddListener(ZigJSAPIWeakPtr listener)
{
	boost::recursive_mutex::scoped_lock lock(s_listenersMutex);
	s_listeners.push_back(listener);
}


const int MAX_USERS = 16;

FB::VariantList ZigJS::PositionToVariant(XnPoint3D pos)
{
	FB::VariantList xyz;
	xyz += pos.X, pos.Y, pos.Z;
	return xyz;
}

FB::VariantList ZigJS::OrientationToVariant(XnMatrix3X3 ori)
{
	FB::VariantList result;
	result.assign(ori.elements, ori.elements + 9);
	return result;
}

FB::VariantList ZigJS::GetJointsList(XnUserID userid)
{
	FB::VariantList result;
	XnSkeletonJointTransformation jointData;

	// quick out if not tracking
	if (!s_users.GetSkeletonCap().IsTracking(userid)) {
		return result;
	}

	// head is the first, right foot the last. probably not the best way.
	for (int i=XN_SKEL_HEAD; i<= XN_SKEL_RIGHT_FOOT; i++) {
		if (s_users.GetSkeletonCap().IsJointAvailable((XnSkeletonJoint)i)) {
			s_users.GetSkeletonCap().GetSkeletonJoint(userid, (XnSkeletonJoint)i, jointData);
			FB::VariantMap joint;
			joint["id"] = i;
			joint["position"] = PositionToVariant(jointData.position.position);
			joint["rotation"] = OrientationToVariant(jointData.orientation.orientation);
			result.push_back(joint);
		}
	}

	return result;
}

FB::VariantList ZigJS::MakeUsersList()
{
	// get the users
	XnUInt16 nUsers = MAX_USERS;
	XnUserID aUsers[MAX_USERS];
	s_users.GetUsers(aUsers, nUsers);

	// construct JS object
	FB::VariantList jsUsers;
	XnPoint3D pos;
	for (int i = 0; i < nUsers; i++) {
		s_users.GetCoM(aUsers[i], pos);
		FB::VariantMap user;
		user["tracked"] = s_users.GetSkeletonCap().IsTracking(aUsers[i]);
		user["centerofmass"] = PositionToVariant(pos);
		user["userid"] = aUsers[i];
		user["joints"] = GetJointsList(aUsers[i]);
		jsUsers += user;
	}

	return jsUsers;
}

unsigned long XN_CALLBACK_TYPE ZigJS::OpenNIThread(void * dont_care)
{
	s_gestures.AddGesture ("Wave",  NULL); //no bounding box
	s_gestures.AddGesture ("Click",  NULL); //no bounding box

	XnStatus nRetVal = s_context.StartGeneratingAll();
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_INFO("xnInit", "fail start generating");
		s_lastFrame = -1;
		return -1;
	} else {
		FBLOG_INFO("xnInit", "ok start generating");
	}
	xn::DepthMetaData md;

	while(!s_quit) {
		s_context.WaitAndUpdateAll();
		if (nRetVal != XN_STATUS_OK) {
			FBLOG_INFO("xnInit", "fail wait & update");
			break;
		} 
		s_depth.GetMetaData(md);
		s_lastFrame = (int)md.FrameID();

		FB::VariantList jsUsers = MakeUsersList();

		// send to listeners
		{
			boost::recursive_mutex::scoped_lock lock(s_listenersMutex);
			for(std::list<ZigJSAPIWeakPtr>::const_iterator i = s_listeners.begin(); i != s_listeners.end(); ) {
				ZigJSAPIPtr realPtr = i->lock();
				if (realPtr) { 
					realPtr->setUsers(jsUsers);
					++i;
				} else {
					s_listeners.erase(i++);
				}
			}
		}
	}
}

void XN_CALLBACK_TYPE ZigJS::GestureRecognizedHandler(xn::GestureGenerator& generator, const XnChar* strGesture, const XnPoint3D* pIDPosition, const XnPoint3D* pEndPosition, void* pCookie)
{
	ZigJS::s_hands.StartTracking(*pEndPosition);
}

void XN_CALLBACK_TYPE ZigJS::HandCreateHandler(xn::HandsGenerator& generator, XnUserID user, const XnPoint3D* pPosition, XnFloat fTime, void* pCookie)
{
	// send to listeners
	{
		boost::recursive_mutex::scoped_lock lock(s_listenersMutex);
		for(std::list<ZigJSAPIWeakPtr>::const_iterator i = s_listeners.begin(); i != s_listeners.end(); ) {
			ZigJSAPIPtr realPtr = i->lock();
			if (realPtr) { 
				realPtr->onHandCreate((int)user,pPosition->X,pPosition->Y,pPosition->Z, (float)fTime);
				++i;
			} else {
				s_listeners.erase(i++);
			}
		}
	}
}

void XN_CALLBACK_TYPE ZigJS::HandUpdateHandler(xn::HandsGenerator& generator, XnUserID user, const XnPoint3D* pPosition, XnFloat fTime, void* pCookie)
{
	// send to listeners
	{
		boost::recursive_mutex::scoped_lock lock(s_listenersMutex);
		for(std::list<ZigJSAPIWeakPtr>::const_iterator i = s_listeners.begin(); i != s_listeners.end(); ) {
			ZigJSAPIPtr realPtr = i->lock();
			if (realPtr) { 
				realPtr->onHandUpdate((int)user,pPosition->X,pPosition->Y,pPosition->Z, (float)fTime);
				++i;
			} else {
				s_listeners.erase(i++);
			}
		}
	}
}

void XN_CALLBACK_TYPE ZigJS::HandDestroyHandler(xn::HandsGenerator& generator, XnUserID user, XnFloat fTime, void* pCookie)
{
	// send to listeners
	{
		boost::recursive_mutex::scoped_lock lock(s_listenersMutex);
		for(std::list<ZigJSAPIWeakPtr>::const_iterator i = s_listeners.begin(); i != s_listeners.end(); ) {
			ZigJSAPIPtr realPtr = i->lock();
			if (realPtr) { 
				realPtr->onHandDestroy((int)user, (float)fTime);
				++i;
			} else {
				s_listeners.erase(i++);
			}
		}
	}
}

void XN_CALLBACK_TYPE ZigJS::OnNewUser(xn::UserGenerator& generator, const XnUserID nUserId, void* pCookie)
{
	generator.GetPoseDetectionCap().StartPoseDetection("Psi", nUserId);

	// send to listeners
	{
		boost::recursive_mutex::scoped_lock lock(s_listenersMutex);
		for(std::list<ZigJSAPIWeakPtr>::const_iterator i = s_listeners.begin(); i != s_listeners.end(); ) {
			ZigJSAPIPtr realPtr = i->lock();
			if (realPtr) { 
				realPtr->onUserEntered(nUserId);
				++i;
			} else {
				s_listeners.erase(i++);
			}
		}
	}
}

void XN_CALLBACK_TYPE ZigJS::OnLostUser(xn::UserGenerator& generator, const XnUserID nUserId, void* pCookie)
{	
		// send to listeners
	{
		boost::recursive_mutex::scoped_lock lock(s_listenersMutex);
		for(std::list<ZigJSAPIWeakPtr>::const_iterator i = s_listeners.begin(); i != s_listeners.end(); ) {
			ZigJSAPIPtr realPtr = i->lock();
			if (realPtr) { 
				realPtr->onUserLeft(nUserId);
				++i;
			} else {
				s_listeners.erase(i++);
			}
		}
	}

}

void XN_CALLBACK_TYPE ZigJS::OnPoseDetected(xn::PoseDetectionCapability& poseDetection, const XnChar* strPose, XnUserID nId, void* pCookie)
{
	// Stop detecting the pose
	poseDetection.StopPoseDetection(nId);

	// Start calibrating
	s_users.GetSkeletonCap().RequestCalibration(nId, TRUE);
}

void XN_CALLBACK_TYPE ZigJS::OnCalibrationStart(xn::SkeletonCapability& skeleton, const XnUserID nUserId, void* pCookie)
{
}

void XN_CALLBACK_TYPE ZigJS::OnCalibrationEnd(xn::SkeletonCapability& skeleton, const XnUserID nUserId, XnBool bSuccess, void* pCookie)
{
	// If this was a successful calibration
	if (bSuccess) {
		// start tracking
		skeleton.StartTracking(nUserId);
		
		// send to listeners
		{
			boost::recursive_mutex::scoped_lock lock(s_listenersMutex);
			for(std::list<ZigJSAPIWeakPtr>::const_iterator i = s_listeners.begin(); i != s_listeners.end(); ) {
				ZigJSAPIPtr realPtr = i->lock();
				if (realPtr) { 
					realPtr->onUserTrackingStarted(nUserId);
					++i;
				} else {
					s_listeners.erase(i++);
				}
			}
		}

		
	} else {
		// Restart pose detection
		s_users.GetPoseDetectionCap().StartPoseDetection("Psi", nUserId);
	}	
}


///////////////////////////////////////////////////////////////////////////////
/// @fn ZigJS::StaticInitialize()
///
/// @brief  Called from PluginFactory::globalPluginInitialize()
///
/// @see FB::FactoryBase::globalPluginInitialize
///////////////////////////////////////////////////////////////////////////////
void ZigJS::StaticInitialize()
{
    // Place one-time initialization stuff here; As of FireBreath 1.4 this should only
    // be called once per process
	XnStatus nRetVal = XN_STATUS_OK;
	nRetVal = s_context.Init();
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_INFO("xnInit", "fail context init");
		return;
	} else {
		FBLOG_INFO("xnInit", "ok context init");
	}
	//TODO: leaking some memory? 
	XnLicense * license = new XnLicense();
	xnOSStrCopy(license->strKey, "0KOIk2JeIBYClPWVnMoRKn5cdY4=", sizeof(license->strKey));
	xnOSStrCopy(license->strVendor, "PrimeSense", sizeof(license->strVendor));
	s_context.AddLicense(*license);

	s_context.CreateAnyProductionTree(XN_NODE_TYPE_DEPTH, NULL, s_depth);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail get depth");
		s_lastFrame = -6;
		return;
	} else {
		FBLOG_DEBUG("xnInit", "ok get depth");
	}
	
	s_context.CreateAnyProductionTree(XN_NODE_TYPE_GESTURE, NULL, s_gestures);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail get gesture");
		s_lastFrame = -6;
		return;
	} else {
		FBLOG_INFO("xnInit", "ok get gesture");
	}
	
	s_context.CreateAnyProductionTree(XN_NODE_TYPE_HANDS, NULL, s_hands);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail get hands");
		s_lastFrame = -6;
		return;
	} else {
		FBLOG_INFO("xnInit", "ok get hands");
	}

	s_context.CreateAnyProductionTree(XN_NODE_TYPE_USER, NULL, s_users);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail get hands");
		s_lastFrame = -6;
		return;
	} else {
		FBLOG_INFO("xnInit", "ok get hands");
	}

	XnCallbackHandle ignore;

	// register to gesture/hands callbacks
	s_gestures.RegisterGestureCallbacks(&ZigJS::GestureRecognizedHandler, NULL, NULL, ignore);
	s_hands.RegisterHandCallbacks(&ZigJS::HandCreateHandler, &ZigJS::HandUpdateHandler, &ZigJS::HandDestroyHandler, NULL, ignore);

	s_users.RegisterUserCallbacks(&ZigJS::OnNewUser, OnLostUser, NULL, ignore);
	s_users.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);
	s_users.GetSkeletonCap().RegisterCalibrationCallbacks(&ZigJS::OnCalibrationStart, &OnCalibrationEnd, NULL, ignore);
	s_users.GetSkeletonCap().SetSmoothing(0.5);
	s_users.GetPoseDetectionCap().RegisterToPoseCallbacks(&ZigJS::OnPoseDetected, NULL, NULL, ignore);

	s_quit = false;
	nRetVal = xnOSCreateThread(OpenNIThread, NULL, &s_threadHandle);
	if (nRetVal != XN_STATUS_OK) {
		FBLOG_DEBUG("xnInit", "fail start thread");
		s_lastFrame = -7;
		return;
	} else {
		FBLOG_DEBUG("xnInit", "ok start thread");
	}
}


///////////////////////////////////////////////////////////////////////////////
/// @fn ZigJS::StaticInitialize()
///
/// @brief  Called from PluginFactory::globalPluginDeinitialize()
///
/// @see FB::FactoryBase::globalPluginDeinitialize
///////////////////////////////////////////////////////////////////////////////
void ZigJS::StaticDeinitialize()
{
    // Place one-time deinitialization stuff here. As of FireBreath 1.4 this should
    // always be called just before the plugin library is unloaded
	s_quit = true;
	XnStatus nRetVal = xnOSWaitForThreadExit(&s_threadHandle, -1); // wait till quit
	if (XN_STATUS_OK != nRetVal) {
		FBLOG_DEBUG("deinit", "failed waiting on thread to quit");
		return;
	}
	s_hands.Release();
	s_gestures.Release();
	s_depth.Release();
	s_context.Release();
	s_listeners.clear();
}



///////////////////////////////////////////////////////////////////////////////
/// @brief  ZigJS constructor.  Note that your API is not available
///         at this point, nor the window.  For best results wait to use
///         the JSAPI object until the onPluginReady method is called
///////////////////////////////////////////////////////////////////////////////
ZigJS::ZigJS()
{
}

///////////////////////////////////////////////////////////////////////////////
/// @brief  ZigJS destructor.
///////////////////////////////////////////////////////////////////////////////
ZigJS::~ZigJS()
{
    // This is optional, but if you reset m_api (the shared_ptr to your JSAPI
    // root object) and tell the host to free the retained JSAPI objects then
    // unless you are holding another shared_ptr reference to your JSAPI object
    // they will be released here.
    releaseRootJSAPI();
    m_host->freeRetainedObjects();
}

void ZigJS::onPluginReady()
{
    // When this is called, the BrowserHost is attached, the JSAPI object is
    // created, and we are ready to interact with the page and such.  The
    // PluginWindow may or may not have already fire the AttachedEvent at
    // this point.
}

void ZigJS::shutdown()
{
    // This will be called when it is time for the plugin to shut down;
    // any threads or anything else that may hold a shared_ptr to this
    // object should be released here so that this object can be safely
    // destroyed. This is the last point that shared_from_this and weak_ptr
    // references to this object will be valid
}

///////////////////////////////////////////////////////////////////////////////
/// @brief  Creates an instance of the JSAPI object that provides your main
///         Javascript interface.
///
/// Note that m_host is your BrowserHost and shared_ptr returns a
/// FB::PluginCorePtr, which can be used to provide a
/// boost::weak_ptr<ZigJS> for your JSAPI class.
///
/// Be very careful where you hold a shared_ptr to your plugin class from,
/// as it could prevent your plugin class from getting destroyed properly.
///////////////////////////////////////////////////////////////////////////////
FB::JSAPIPtr ZigJS::createJSAPI()
{
    // m_host is the BrowserHost
    ZigJSAPIPtr newJSAPI = boost::make_shared<ZigJSAPI>(FB::ptr_cast<ZigJS>(shared_from_this()), m_host);
	ZigJS::AddListener(newJSAPI);
	return newJSAPI;
}

bool ZigJS::onMouseDown(FB::MouseDownEvent *evt, FB::PluginWindow *)
{
    //printf("Mouse down at: %d, %d\n", evt->m_x, evt->m_y);
    return false;
}

bool ZigJS::onMouseUp(FB::MouseUpEvent *evt, FB::PluginWindow *)
{
    //printf("Mouse up at: %d, %d\n", evt->m_x, evt->m_y);
    return false;
}

bool ZigJS::onMouseMove(FB::MouseMoveEvent *evt, FB::PluginWindow *)
{
    //printf("Mouse move at: %d, %d\n", evt->m_x, evt->m_y);
    return false;
}
bool ZigJS::onWindowAttached(FB::AttachedEvent *evt, FB::PluginWindow *)
{
    // The window is attached; act appropriately
    return false;
}

bool ZigJS::onWindowDetached(FB::DetachedEvent *evt, FB::PluginWindow *)
{
    // The window is about to be detached; act appropriately
    return false;
}

