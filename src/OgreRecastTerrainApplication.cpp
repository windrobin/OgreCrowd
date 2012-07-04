#include "include/OgreRecastTerrainApplication.h"
#include <Terrain/OgreTerrain.h>

OgreRecastTerrainApplication::OgreRecastTerrainApplication()
    : mTerrainsImported(false),
      mTerrainGroup(NULL),
      mTerrainGlobals(NULL)
{
}

OgreRecastTerrainApplication::~OgreRecastTerrainApplication()
{
}


void OgreRecastTerrainApplication::createScene()
{
    // Basic scene setup
    Ogre::Vector3 lightdir(0.55, -0.3, 0.75);
    lightdir.normalise();
    Ogre::Light* light = mSceneMgr->createLight("MainLight");
    light->setType(Ogre::Light::LT_DIRECTIONAL);
    light->setDirection(lightdir);
    light->setDiffuseColour(Ogre::ColourValue::White);
    light->setSpecularColour(Ogre::ColourValue(0.4, 0.4, 0.4));
    mSceneMgr->setAmbientLight(Ogre::ColourValue(0.2, 0.2, 0.2));
    mCamera->setPosition(-3161.99, 866.029, -6214.35);
    mCamera->setOrientation(Ogre::Quaternion(0.21886, -0.0417, -0.9576, -0.1826));
    mCamera->setNearClipDistance(0.1);
    mCamera->setFarClipDistance(50000);
    if (mRoot->getRenderSystem()->getCapabilities()->hasCapability(Ogre::RSC_INFINITE_FAR_PLANE))
    {
        mCamera->setFarClipDistance(0);   // enable infinite far clip distance if we can
    }

    // Set viewport color to blue (makes bounding boxes more visible)
    Ogre::Viewport *vp = mWindow->getViewport(0);
    vp->setBackgroundColour(Ogre::ColourValue(13.0/255,221.0/255,229.0/255));


    // Set the default ray query mask for any created scene object
    Ogre::MovableObject::setDefaultQueryFlags(DEFAULT_MASK);

    // Create navigateable terrain
    mTerrainGlobals = OGRE_NEW Ogre::TerrainGlobalOptions();
    mTerrainGroup = OGRE_NEW Ogre::TerrainGroup(mSceneMgr, Ogre::Terrain::ALIGN_X_Z, 513, 12000.0f);
    mTerrainGroup->setFilenameConvention(Ogre::String("OgreRecastTerrain"), Ogre::String("dat"));
    mTerrainGroup->setOrigin(Ogre::Vector3::ZERO);
    configureTerrainDefaults(light);

    for (long x = 0; x <= 0; ++x)
        for (long y = 0; y <= 0; ++y)
            defineTerrain(x, y);

    // sync load since we want everything in place when we start
    mTerrainGroup->loadAllTerrains(true);

    if (mTerrainsImported)
    {
        Ogre::TerrainGroup::TerrainIterator ti = mTerrainGroup->getTerrainIterator();
        while(ti.hasMoreElements())
        {
            Ogre::Terrain* t = ti.getNext()->instance;
            initBlendMaps(t);
        }
    }

    mTerrainGroup->freeTemporaryResources();

// TODO extract recast/detour parameters out of their wrappers, use config classes
    // RECAST (navmesh creation)
    InputGeom *geom = new InputGeom(mTerrainGroup);
    // Debug draw the recast bounding box around the terrain tile
    ConvexVolume bb = ConvexVolume(geom->getBoundingBox());
    InputGeom::drawConvexVolume(&bb, mSceneMgr);
    // Verify rasterized terrain mesh
//    geom->debugMesh(mSceneMgr);
    if(SINGLE_NAVMESH) {
        // Simple recast navmesh build example
        // For large terrain meshes this is not recommended, as the build takes a very long time

        // Initialize custom navmesh parameters
        OgreRecastConfigParams recastParams = OgreRecastConfigParams();
        recastParams.setCellSize(50);
        recastParams.setCellHeight(6);
        recastParams.setAgentMaxSlope(45);
        recastParams.setAgentHeight(2.5);
        recastParams.setAgentMaxClimb(15);
        recastParams.setAgentRadius(0.5);
        recastParams.setEdgeMaxLen(2);
        recastParams.setEdgeMaxError(1.3);
        recastParams.setRegionMinSize(50);
        recastParams.setRegionMergeSize(20);
        recastParams.setDetailSampleDist(5);
        recastParams.setDetailSampleMaxError(5);

        mRecast = new OgreRecast(mSceneMgr, recastParams);

        if(mRecast->NavMeshBuild(geom)) {
            mRecast->drawNavMesh();
        } else {
            Ogre::LogManager::getSingletonPtr()->logMessage("ERROR: could not generate useable navmesh from mesh.");
            return;
        }

    // DetourTileCache navmesh creation
    } else {
        // More advanced: use DetourTileCache to build a tiled and cached navmesh that can be updated with dynamic obstacles at runtime.

        // Initialize custom navmesh parameters
        OgreRecastConfigParams recastParams = OgreRecastConfigParams();
        recastParams.setCellSize(0.3);
        recastParams.setCellHeight(0.5);
        recastParams.setAgentMaxSlope(45);
        recastParams.setAgentHeight(2.5);
        recastParams.setAgentMaxClimb(15);
        recastParams.setAgentRadius(0.5);
//        recastParams.setEdgeMaxLen(2);
//        recastParams.setEdgeMaxError(1.3);
//        recastParams.setRegionMinSize(50);
//        recastParams.setRegionMergeSize(20);
//        recastParams.setDetailSampleDist(5);
//        recastParams.setDetailSampleMaxError(5);

        mRecast = new OgreRecast(mSceneMgr, recastParams);

        // Optimize debug drawing batch count
        OgreRecast::STATIC_GEOM_DEBUG = true;

        // Make it quiet to avoid spamming the log
        OgreRecast::VERBOSE = false;

        mDetourTileCache = new OgreDetourTileCache(mRecast, 48);
        mDetourTileCache->configure(geom);
        Ogre::AxisAlignedBox areaToLoad;
//        areaToLoad.setMaximum(geom->getBoundingBox().getMaximum());
//        areaToLoad.setMinimum(Ogre::Vector3(5000, 0, 5000));

//        areaToLoad.setMinimum(geom->getBoundingBox().getMinimum());
//        areaToLoad.setMaximum(Ogre::Vector3(-5800, geom->getBoundingBox().getMaximum().y, -5800));

        areaToLoad.setMinimum(Ogre::Vector3(-2400, 0, -6000));
        areaToLoad.setMaximum(Ogre::Vector3(-2300, geom->getBoundingBox().getMaximum().y, -5800));
        mDetourTileCache->buildTiles(geom, &areaToLoad);    // Only build a few tiles
        /*
        if(mDetourTileCache->TileCacheBuild(geom)) {
            mDetourTileCache->drawNavMesh();
        } else {
            Ogre::LogManager::getSingletonPtr()->logMessage("ERROR: could not generate useable navmesh from mesh using detourTileCache.");
            return;
        }
        */
    }


    // You can save out the input geom to wavefront .obj like so:
    // This can be used, for example, for testing in the official demo that comes with recast
//    if(mRecast->m_geom)
//        mRecast->m_geom->writeObj("terrainTestObj.obj");
//    else
//        mDetourTileCache->m_geom->writeObj("terrainTestObj.obj");


    // DETOUR (pathfinding)
    // Do a pathing between two random points on the navmesh and draw the path
    // Note that because we are using DetourCrowd we will not be doing pathfinds directly, DetourCrowd
    // will do this for us.
    int pathNb = 0;     // The index number for the slot in which the found path is to be stored
    int targetId = 0;   // Number identifying the target the path leads to
    Ogre::Vector3 beginPos = mRecast->getRandomNavMeshPoint();
    Ogre::Vector3 endPos = mRecast->getRandomNavMeshPoint();
    if(OgreRecastApplication::mDebugDraw)
        calculateAndDrawPath(beginPos, endPos, pathNb, targetId);


    // DETOUR CROWD (local steering for independent agents)
    // Create a first agent that always starts at begin position
    mDetourCrowd = new OgreDetourCrowd(mRecast);
    Character *character = createCharacter("Agent0", beginPos);    // create initial agent at start marker
    if(!HUMAN_CHARACTERS)
        character->getEntity()->setMaterialName("Cylinder/LightBlue");  // Give the first agent a different color
    setDestinationForAllAgents(endPos, false);  // Move all agents in crowd to destination







    //-------------------------------------------------
    // The rest of this method is specific to the demo

    // PLACE PATH BEGIN AND END MARKERS
    if(mDebugDraw) {
        beginPos.y = beginPos.y + mRecast->m_navMeshOffsetFromGround;
        endPos.y = endPos.y + mRecast->m_navMeshOffsetFromGround;
    }
    getOrCreateMarker("BeginPos", "Cylinder/Wires/DarkGreen")->setPosition(beginPos);
    getOrCreateMarker("EndPos", "Cylinder/Wires/Brown")->setPosition(endPos);


    // ADJUST CAMERA MOVING SPEED (default is 150)
    mCameraMan->setTopSpeed(1000);


    // SETUP RAY SCENE QUERYING AND DEBUG DRAWING
    // Used for mouse picking begin and end markers and determining the position to add new agents
    // Add navmesh to separate querying group that we will use
    mNavMeshNode = (Ogre::SceneNode*)mSceneMgr->getRootSceneNode()->getChild("RecastSN");
    for (int i = 0; i < mNavMeshNode->numAttachedObjects(); i++) {
        Ogre::MovableObject *obj = mNavMeshNode->getAttachedObject(i);
        obj->setQueryFlags(NAVMESH_MASK);
    }

// TODO raycast terrain
/*
    if (RAYCAST_SCENE)
        mapE->setQueryFlags(NAVMESH_MASK);
*/

    if(!OgreRecastApplication::mDebugDraw)
        mNavMeshNode->setVisible(false); // Even though we make it invisible, we still keep the navmesh entity in the scene to do ray intersection tests


    // CREATE CURSOR OVERLAY
    mCrosshair = Ogre::OverlayManager::getSingletonPtr()->getByName("GUI/Crosshair");
    mCrosshair->show(); // Show a cursor in the center of the screen


    // SETUP AGENT STEERING DEMO
    // Create a chase camera to follow the first agent
    Ogre::SceneNode *node = mCharacters[0]->getNode();
    mChaseCam = mSceneMgr->createCamera("AgentFollowCamera");
    mChaseCam->setNearClipDistance(0.1);
    node->attachObject(mChaseCam);
    mChaseCam->setPosition(0, mDetourCrowd->getAgentHeight(), mDetourCrowd->getAgentRadius()*4);
    mChaseCam->pitch(Ogre::Degree(-15));
    mChaseCam->setAspectRatio(Ogre::Real(vp->getActualWidth()) / Ogre::Real(vp->getActualHeight()));
}

void OgreRecastTerrainApplication::configureTerrainDefaults(Ogre::Light* light)
{
    // Configure global
    mTerrainGlobals->setMaxPixelError(8);   // Precision of terrain
    // testing composite map
    mTerrainGlobals->setCompositeMapDistance(3000);     // Max distance of terrain to be rendered

    // Important to set these so that the terrain knows what to use for derived (non-realtime) data
    mTerrainGlobals->setLightMapDirection(light->getDerivedDirection());
    mTerrainGlobals->setCompositeMapAmbient(mSceneMgr->getAmbientLight());
    mTerrainGlobals->setCompositeMapDiffuse(light->getDiffuseColour());

    // Configure default import settings for if we use imported image
    Ogre::Terrain::ImportData& defaultimp = mTerrainGroup->getDefaultImportSettings();
    defaultimp.terrainSize = 513;
    defaultimp.worldSize = 12000.0f;
    defaultimp.inputScale = 600; // due terrain.png is 8 bpp
    defaultimp.minBatchSize = 33;
    defaultimp.maxBatchSize = 65;

    // textures
    defaultimp.layerList.resize(3);
    defaultimp.layerList[0].worldSize = 100;
    defaultimp.layerList[0].textureNames.push_back("dirt_grayrocky_diffusespecular.dds");
    defaultimp.layerList[0].textureNames.push_back("dirt_grayrocky_normalheight.dds");
    defaultimp.layerList[1].worldSize = 30;
    defaultimp.layerList[1].textureNames.push_back("grass_green-01_diffusespecular.dds");
    defaultimp.layerList[1].textureNames.push_back("grass_green-01_normalheight.dds");
    defaultimp.layerList[2].worldSize = 200;
    defaultimp.layerList[2].textureNames.push_back("growth_weirdfungus-03_diffusespecular.dds");
    defaultimp.layerList[2].textureNames.push_back("growth_weirdfungus-03_normalheight.dds");
}


/**
  * Flipping is done to imitate seamless terrain so you can create unlimited terrain using a
  * single 513x513 heightmap, it's just a trick.
  **/
void getTerrainImage(bool flipX, bool flipY, Ogre::Image& img)
{
    img.load("terrain.png", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    if (flipX)
        img.flipAroundY();
    if (flipY)
        img.flipAroundX();

}

void OgreRecastTerrainApplication::defineTerrain(long x, long y)
{
    Ogre::String filename = mTerrainGroup->generateFilename(x, y);
    if (Ogre::ResourceGroupManager::getSingleton().resourceExists(mTerrainGroup->getResourceGroup(), filename))
    {
        mTerrainGroup->defineTerrain(x, y);
    }
    else
    {
        Ogre::Image img;
        getTerrainImage(x % 2 != 0, y % 2 != 0, img);
        mTerrainGroup->defineTerrain(x, y, &img);
        mTerrainsImported = true;
    }
}


void OgreRecastTerrainApplication::initBlendMaps(Ogre::Terrain* terrain)
{
    Ogre::TerrainLayerBlendMap* blendMap0 = terrain->getLayerBlendMap(1);
    Ogre::TerrainLayerBlendMap* blendMap1 = terrain->getLayerBlendMap(2);
    Ogre::Real minHeight0 = 70;
    Ogre::Real fadeDist0 = 40;
    Ogre::Real minHeight1 = 70;
    Ogre::Real fadeDist1 = 15;
    float* pBlend0 = blendMap0->getBlendPointer();
    float* pBlend1 = blendMap1->getBlendPointer();
    for (Ogre::uint16 y = 0; y < terrain->getLayerBlendMapSize(); ++y)
    {
        for (Ogre::uint16 x = 0; x < terrain->getLayerBlendMapSize(); ++x)
        {
            Ogre::Real tx, ty;

            blendMap0->convertImageToTerrainSpace(x, y, &tx, &ty);
            Ogre::Real height = terrain->getHeightAtTerrainPosition(tx, ty);
            Ogre::Real val = (height - minHeight0) / fadeDist0;
            val = Ogre::Math::Clamp(val, (Ogre::Real)0, (Ogre::Real)1);
            *pBlend0++ = val;

            val = (height - minHeight1) / fadeDist1;
            val = Ogre::Math::Clamp(val, (Ogre::Real)0, (Ogre::Real)1);
            *pBlend1++ = val;
        }
    }
    blendMap0->dirty();
    blendMap1->dirty();
    blendMap0->update();
    blendMap1->update();
}


bool OgreRecastTerrainApplication::keyPressed( const OIS::KeyEvent &arg )
{
    if(  arg.key == OIS::KC_O
//      || arg.key == OIS::KC_BACK
//      || arg.key == OIS::KC_DELETE
      || arg.key == OIS::KC_K
      || arg.key == OIS::KC_I)
        return BaseApplication::keyPressed(arg);    // Avoid these features from the dungeon demo

    // Use X to test terrain height
    if(arg.key == OIS::KC_X) {
        Ogre::Ray cursorRay = mCamera->getCameraToViewportRay(0.5, 0.5);

        // Perform the scene query
        Ogre::TerrainGroup::RayResult result = mTerrainGroup->rayIntersects(cursorRay);
        if(result.hit) {
            Ogre::Real terrainHeight = result.position.y;
            Ogre::LogManager::getSingletonPtr()->logMessage("Terrain height: "+ Ogre::StringConverter::toString(terrainHeight));
        }
    }

    // Resort to original recast demo functionality
    return OgreRecastApplication::keyPressed(arg);
}

void OgreRecastTerrainApplication::destroyScene(void)
{
    OGRE_DELETE mTerrainGroup;
    OGRE_DELETE mTerrainGlobals;
}

bool OgreRecastTerrainApplication::mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id )
{
    // TODO extract submethod

    // Make sure that any redrawn navmesh tiles have the proper query mask
    for (int i = 0; i < mNavMeshNode->numAttachedObjects(); i++) {
        Ogre::MovableObject *obj = mNavMeshNode->getAttachedObject(i);
        obj->setQueryFlags(NAVMESH_MASK);
    }

    // Set static geometry query mask
    if (OgreRecast::STATIC_GEOM_DEBUG) {
        Ogre::StaticGeometry::RegionIterator it = mSceneMgr->getStaticGeometry("NavmeshDebugStaticGeom")->getRegionIterator();
        while (it.hasMoreElements())
        {
            Ogre::StaticGeometry::Region* region = it.getNext();
            region->setQueryFlags(NAVMESH_MASK);
        }
    }

    Ogre::Vector3 rayHitPoint;
    if(queryCursorPosition(rayHitPoint)) {

        Ogre::SceneNode *markerNode = NULL;

        if(id == OIS::MB_Left) {
            markerNode = getOrCreateMarker("EndPos", "Cylinder/Wires/Brown");

            if(mApplicationState != SIMPLE_PATHFIND)
                mCharacters[0]->updateDestination(rayHitPoint, false);  // Update destination of first agent only
        }

        if(id == OIS::MB_Right && mApplicationState == SIMPLE_PATHFIND) {
            markerNode = getOrCreateMarker("BeginPos", "Cylinder/Wires/DarkGreen");
        }

        if(markerNode != NULL) {
            if(mDebugDraw)
                rayHitPoint.y = rayHitPoint.y + mRecast->m_navMeshOffsetFromGround;
            markerNode->setPosition(rayHitPoint);
        }

        if(mApplicationState == SIMPLE_PATHFIND) {
            drawPathBetweenMarkers(1,1);    // Draw navigation path (in DRAW_DEBUG) and begin marker
            UpdateAllAgentDestinations();   //Steer all agents to set destination
        }
    }

    return BaseApplication::mousePressed(arg, id);
}


bool OgreRecastTerrainApplication::queryCursorPosition(Ogre::Vector3 &rayHitPoint, unsigned long queryflags, Ogre::MovableObject **rayHitObject)
{
    if (OgreRecast::STATIC_GEOM_DEBUG || OgreRecastApplication::RAYCAST_SCENE) {
        // Raycast terrain
        Ogre::Ray cursorRay = mCamera->getCameraToViewportRay(0.5, 0.5);

        // Perform the scene query
        Ogre::TerrainGroup::RayResult result = mTerrainGroup->rayIntersects(cursorRay);
        if(result.hit) {
            // Queried point was not on navmesh, find nearest point on the navmesh
            mRecast->findNearestPointOnNavmesh(result.position, rayHitPoint);
            return true;
        } else {
            return false;
        }
    } else {
        return OgreRecastApplication::queryCursorPosition(rayHitPoint, queryflags, rayHitObject);
    }
}


Character* OgreRecastTerrainApplication::createCharacter(Ogre::String name, Ogre::Vector3 position)
{
    Character* character = OgreRecastApplication::createCharacter(name, position);
    // Enable terrain clipping for character
    character->clipToTerrain(mTerrainGroup);
    return character;
}

bool OgreRecastTerrainApplication::frameRenderingQueued(const Ogre::FrameEvent &evt)
{
    // Update navmesh debug drawing
    mRecast->update();

    return OgreRecastApplication::frameRenderingQueued(evt);
}

void OgreRecastTerrainApplication::setDebugVisibility(bool visible)
{
    OgreRecastApplication::setDebugVisibility(visible);

    if(OgreRecast::STATIC_GEOM_DEBUG) {
        Ogre::StaticGeometry *sg = mSceneMgr->getStaticGeometry("NavmeshDebugStaticGeom");
        sg->setVisible(visible);
    }
}
