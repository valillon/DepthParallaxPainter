
#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
    ofSetLogLevel(OF_LOG_NOTICE);
    ofSetVerticalSync(true);
    ofFill();
    ofSetFrameRate(30);
    glPointSize(1);
    canvas.render = OF_MESH_WIREFRAME;
    
#ifdef LEAP_MOTION_ON
    hand_id = 0;    // 0-righthand 1-lefthand;
    finger_id = 2;
    leap.open();
    leap.setMappingX(-100, 100, -ofGetScreenWidth()  / 2, ofGetScreenWidth()  / 2);
    leap.setMappingY(  50, 100, -ofGetScreenHeight() / 2, ofGetScreenHeight() / 2);
    leap.setMappingZ(   0, 100, -ofGetScreenWidth()  / 2, ofGetScreenWidth()  / 2);
#endif
    
    resetCamera();
    example = 0, nPlanes = DEPTH_INIT_PLANES;
    loadExample();
}

//--------------------------------------------------------------
void ofApp::update()
{
    camera.move(camera.speed);
    
#ifdef LEAP_MOTION_ON
    hands = leap.getSimpleHands();
    if ( leap.isFrameNew() && hands.size() )
    {
        if ( hands[hand_id].fingers.size() >= finger_id )
        {
            ofVec3f finger_orientation = hands[hand_id].handPos - hands[hand_id].fingers[finger_id].pos;
            camera.tiltDeg( -finger_orientation.y * LEAP_MOTION_SENSIBILITY );
            camera.panDeg(  -finger_orientation.x * LEAP_MOTION_SENSIBILITY );
            camera.setPosition( camera.getPosition() - camera.getLookAtDir() * hands[hand_id].handPos.z * LEAP_MOTION_SENSIBILITY * 5 );
            camera.rollDeg( ( hands[hand_id].handPos.y - hands[hand_id].fingers[0].pos.y ) * LEAP_MOTION_SENSIBILITY);
        }
    }
    leap.markFrameAsOld();
#endif
}

//--------------------------------------------------------------
void ofApp::draw()
{
    ofDisableDepthTest();
    ofBackgroundGradient(central_color, edge_color, OF_GRADIENT_CIRCULAR);
    //ofBackground(central_color * 0.6 - edge_color * 0.4);
    ofEnableDepthTest();
    camera.begin();
    canvas.draw();
    camera.end();
    ofDisableDepthTest();
    
    if ( ! bHelp ) return;
    
    ofPushStyle();
    ofSetColor(255);
    string msg = "Source "              + ofToString(image.getWidth()) + " x " + ofToString(image.getHeight());
    msg += "\nFps: "                    + ofToString(ofGetFrameRate(), 2);
    msg += "\nCamera position: "        + ofToString(camera.getPosition(), 2);
    msg += "\nCamera Speed 'arrows': "  + ofToString(camera.speed, 2);
    msg += "\nCamera reset 'return'";
    msg += "\nCamera orbit 'spacebar'";
    msg += "\nFocal distance 'q,w': "   + ofToString(camera.focal, 2);
    msg += "\nExtrusion 'e,r': "        + ofToString(camera.extrusion, 2);
    msg += "\nInpaint background 'i': " + ofToString(bInpaint);
    msg += "\nDepth planes 'n,m': "     + ofToString(nPlanes, 2);
    msg += "\nShow depth 'd': "         + ofToString(bDepth);
    msg += "\nSwitch depth mode 'b': "  + ofToString(bPlanes);
    msg += "\nRerun depth planes '.'";
    msg += "\nRender mode 'z,x,c'";

#ifdef LEAP_MOTION_ON
    msg += leap.isConnected() ? "\nLEAP connected!" : "\nLEAP disconnected o_0";
#endif
    msg += "\nHelp 'h'";
    ofDrawBitmapString(msg, 10, 10);
    ofSetColor(255,0,0);
    if ( ! bLoaded ) ofDrawBitmapString("Error loading sources o_O", 10, ofGetWindowHeight()-20);
    ofPopStyle();
}
//--------------------------------------------------------------
void ofApp::updateCanvas()
{
    if ( ! bLoaded )                                return;
    if ( ! image.getPixels().isAllocated() )        return;
    if ( ! image_depth.getPixels().isAllocated() )  return;
    
    canvas.clear();
    
    if ( bPlanes )
    {
        // The variable 'means' should contain k-centroids after unsupervised clustering
        // For some strange reason the clustering works fine but the output centroids are sometimes entangled
        ofImage planes;
        cv::Mat means = ofxCv::kmeans(image_depth, planes, nPlanes);
        cv::Mat planesCV = ofxCv::toCv(planes);
        planesCV.convertTo(planesCV, CV_32F);
        
        for ( int k = 0; k < means.rows; ++k )
        {
            int p = means.at<uchar>(k);
            cv::Mat maskCV = planesCV.clone();
            
            if ( bInpaint && k == 0)
            {
                // Take the first plane in principle being at the backpground and inpaint empty areas
                maskCV.setTo(1, planesCV != p);
                maskCV.setTo(0, planesCV == p);
                ofImage inpainted, mask;
                maskCV.convertTo(maskCV, CV_8UC1);
                ofxCv::toOf(maskCV, mask);
                ofxCv::inpaint(image, mask, inpainted);
                ofImage inpainted_depth;
                ofxCv::inpaint(image_depth, mask, inpainted_depth); // depth should be inpainted too
                addCanvasPlane(inpainted, inpainted_depth, cv::Mat::ones(maskCV.size(), CV_32F));
                
            } else {
                
                // Add remaining planes with their corresponding masks
                maskCV.setTo(-1, planesCV != p);   // use -1 to mask pixels
                addCanvasPlane(image, image_depth, maskCV);
            }
        }
        
    } else addCanvasPlane(image, image_depth);
    
    canvas.updateLimits();
}

// src: image containing pixels colors
// depth: an image containing depth information
// mask: an image in which -1 stands for masked pixels and positive values stand for target depth values
void ofApp::addCanvasPlane(ofImage src, ofImage depth, cv::Mat mask)
{
    ofLogNotice() << "Updating mesh " << canvas.meshes.size() << " in depth mode " << bPlanes;
    
    unsigned char * sData = src.getPixels().getData();
    unsigned char * dData = depth.getPixels().getData();
    float * mData = mask.empty() ? nullptr : mask.ptr<float>(0);
        
    float plane_depth = 0;
    std::vector<std::vector<int>> indexes(canvas.height, std::vector<int>(canvas.width, -1));
    if ( ! mask.empty() )
    {
        int i = 0;
        for ( unsigned int y = 0; y < canvas.height; ++y )
        {
            for ( unsigned int x = 0; x < canvas.width; ++x )
            {
                int pos = x + y * canvas.width;
                if ( mData[ pos ] < 0 ) continue;
                plane_depth += dData[ pos ];
                indexes[y][x] = i++;
            }
        }
        plane_depth /= i;
    }
    
    glm::vec2 center = glm::vec2( canvas.width  * 0.5, canvas.height * 0.5 );
    ofMesh plane;
    for ( unsigned int y = 0; y < canvas.height; ++y )
    {
        for ( unsigned int x = 0; x < canvas.width; ++x )
        {
            int pos = x + y * canvas.width;
            if ( ! mask.empty() && mData[ pos ] < 0 ) continue;
            
            float d = mask.empty() ? dData[ pos ] : plane_depth;

            // Color
            pos *= 3;
            int r = sData[ pos ];
            int g = sData[ pos + 1 ];
            int b = sData[ pos + 2 ];
            int a = 255;
            ofColor color = bDepth ? ofColor(d,d,d,a) : ofColor(r,g,b,a);
            
            // 3D Location
            float w = x - center.x;
            float h = y - center.y;
            ofVec3f v(w,h,-camera.focal);
            float n = v.length();
            v /= n; // v.normalize();
            v *= ( n + d * camera.extrusion ) - ofVec3f(0,0,camera.focal);
            
            plane.addVertex(v);
            plane.addColor(color);
            
            // Store depths
            canvas.vertexes.push_back(v);

            if ( x == canvas.width-1 || y == canvas.height-1 ) continue;
                     
            int pos0, pos1, pos2, pos3;
            
            if ( ! mask.empty() )
            {
                pos0 = indexes[y  ][x  ];
                pos1 = indexes[y+1][x  ];
                pos2 = indexes[y  ][x+1];
                pos3 = indexes[y+1][x+1];
                
            } else {
                
                pos0 =  x    +  y    * canvas.width;
                pos1 = (x+1) +  y    * canvas.width;
                pos2 =  x    + (y+1) * canvas.width;
                pos3 = (x+1) + (y+1) * canvas.width;
            }
            
            if ( pos0 < 0 || pos1 < 0 || pos2 < 0 ) continue;
            
            plane.addIndex( pos0 );
            plane.addIndex( pos1 );
            plane.addIndex( pos2 );
            
            if (canvas.render != OF_MESH_FILL) continue;
            if ( pos3 < 0 ) continue;
            
            plane.addIndex( pos1 );
            plane.addIndex( pos2 );
            plane.addIndex( pos3 );
        }
    }
    if ( plane.hasVertices() ) canvas.addMesh(plane);
}
//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
    switch (key)
    {
        case 'h': bHelp = ! bHelp;                                          break;
        case 'd': bDepth = ! bDepth;                    updateCanvas();     break;
        case 'b': bPlanes = ! bPlanes;                  updateCanvas();     break;
        case 'i': bInpaint = ! bInpaint;                updateCanvas();     break;
        case 'z': canvas.render = OF_MESH_POINTS;       updateCanvas();     break;
        case 'x': canvas.render = OF_MESH_WIREFRAME;    updateCanvas();     break;
        case 'c': canvas.render = OF_MESH_FILL;         updateCanvas();     break;
        case 'e': camera.extrusion += 0.1;              updateCanvas();     break;
        case 'r': camera.extrusion -= 0.1;              updateCanvas();     break;
        case 'q': camera.focal += 500;                  updateCanvas();     break;
        case 'w': camera.focal -= 500;                  updateCanvas();     break;
        case 'n': nPlanes = max(nPlanes-1, 0);          updateCanvas();     break;
        case 'm': nPlanes = min(nPlanes+1, 10);         updateCanvas();     break;
        case '.': updateCanvas();                                           break;
        case ']': ++example; loadExample();                                 break;
        case '[': --example; loadExample();                                 break;
        case ' ': camera.orbit = ! camera.orbit;                            break;
        case OF_KEY_RETURN:     resetCamera();                              break;
        case OF_KEY_RIGHT:      camera.speed.x += 0.1;                      break;
        case OF_KEY_LEFT:       camera.speed.x -= 0.1;                      break;
        case OF_KEY_UP:
            if (ofGetKeyPressed(OF_KEY_SHIFT))   camera.speed.z += 0.1;
            else                                 camera.speed.y -= 0.1;
            break;
        case OF_KEY_DOWN:
            if (ofGetKeyPressed(OF_KEY_SHIFT))   camera.speed.z -= 0.1;
            else                                 camera.speed.y += 0.1;
            break;
        default: break;
    }
}
void ofApp::loadExample()
{
    string image_name, depth_name;
    
    example = example < 0 ? 9 + example : example % 9;
    switch (example)
    {
        case 0:
            image_name = "velazquez_meninas.jpg";
            depth_name = "velazquez_meninas_depth.png";
            break;
        case 1:
            image_name = "goya_mayo.png";
            depth_name = "goya_mayo_depth.png";
            break;
        case 2:
            image_name = "degas_dancers.png";
            depth_name = "degas_dancers_depth.png";
            break;
        case 3:
            image_name = "millet_angelus.png";
            depth_name = "millet_angelus_depth.png";
            break;
        case 4:
            image_name = "picasso_azul.png";
            depth_name = "picasso_azul_depth.png";
            break;
        case 5:
            image_name = "seurat.png";
            depth_name = "seurat_depth.png";
            break;
        case 6:
            image_name = "pujola1.png";
            depth_name = "pujola1_depth.png";
            break;
        case 7:
            image_name = "pujola2.png";
            depth_name = "pujola2_depth.png";
            break;
        case 8:
            image_name = "pujola3.png";
            depth_name = "pujola3_depth.png";
            break;
        default:
            break;
    }
            
    bLoaded = image.load(image_name);
    bLoaded &= image_depth.load(depth_name);
    
    if ( ! bLoaded ) { ofLogError() << "Resource not found"; return; }

    // image.resize(image.getWidth() * 0.5, image.getHeight() * 0.5);
    image_depth.resize(image.getWidth(), image.getHeight());
    image_depth.setImageType(OF_IMAGE_GRAYSCALE);
    image.setImageType(OF_IMAGE_COLOR);
    
    canvas.width = image.getWidth();
    canvas.height = image.getHeight();

    central_color = edge_color = image.getPixels().getColor( canvas.width * ( 1 + canvas.height * 0.5 ) );
    central_color.setBrightness(55);
    edge_color.setBrightness(15);

    updateCanvas();
}
void ofApp::resetCamera()
{
    camera.orbit = 0;
    camera.speed = glm::vec3(0.f,0.f,0.f);
    camera.extrusion = CANVAS_INIT_EXTRUSION;
    camera.focal = CAMERA_INIT_FOCAL;
    camera.setScale(-1,-1, 1);
    camera.setFov(70);
    camera.setPosition(glm::vec3( 0., 0., CAMERA_INIT_ZPOS ));
    glm::vec3 center(0, 0, (canvas.limits.far.z - canvas.limits.near.z) * 0.5 + canvas.limits.near.z);
    glm::vec3 up(0,1,0);
    camera.lookAt(center,up);
}
//--------------------------------------------------------------
void ofApp::exit()
{
#ifdef LEAP_MOTION
    leap.close();
#endif
}
//------------------------------------------------------------
void ofApp::keyReleased(int key){}
void ofApp::mouseMoved(int x, int y){}
void ofApp::mouseDragged(int x, int y, int button){}
void ofApp::mousePressed(int x, int y, int button){}
void ofApp::mouseReleased(int x, int y, int button){}
void ofApp::mouseEntered(int x, int y){}
void ofApp::mouseExited(int x, int y){}
void ofApp::windowResized(int w, int h){}
void ofApp::gotMessage(ofMessage msg){}
void ofApp::dragEvent(ofDragInfo dragInfo){}
