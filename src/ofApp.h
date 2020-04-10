/*
The code in this repository is available under the MIT License.

Copyright (c) 2019 - Rafael Redondo

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#pragma once

//#define LEAP_MOTION_ON

#define CAMERA_POSE_ROTATION_SPEED      5E-3
#define CAMERA_POSE_ELEVATION           0.22    // [0,1]
#define CAMERA_POSE_EXCENTRICITY        0.5     // [0,1]

#define DEPTH_INIT_PLANES               3
#define CANVAS_INIT_EXTRUSION           1.0
#define CAMERA_INIT_FOCAL               9000
#define CAMERA_INIT_ZPOS                -1200

#include "ofMain.h"
#include "ofxCv.h"

#ifdef LEAP_MOTION_ON
#define LEAP_MOTION_SENSIBILITY         4E-4    // [0,..)
#include "ofxLeapMotion.h"
#endif

class ofApp : public ofBaseApp
{
public:
    
    void setup();
    void update();
    void draw();
    
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseEntered(int x, int y);
    void mouseExited(int x, int y);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
    void exit();
    
    void loadExample();
    void updateCanvas();
    void addCanvasPlane(ofImage depth, cv::Mat mask = cv::Mat());
    
    class Canvas
    {
        public:
        int width, height;
        ofPolyRenderMode render;
        vector<ofMesh> meshes;
        vector<glm::vec3> vertexes;
        struct Limits {
            glm::vec3 far, near;
        } limits;
        void updateLimits() {
            limits.far  = *std::max_element(vertexes.begin(),vertexes.end(),[](glm::vec3 v1,glm::vec3 v2){return v1.z<v2.z;});
            limits.near = *std::min_element(vertexes.begin(),vertexes.end(),[](glm::vec3 v1,glm::vec3 v2){return v1.z<v2.z;});
        }
        void draw() { if ( ! meshes.empty() ) for (auto m : meshes) m.draw(render); }
        void addMesh(ofMesh mesh) { meshes.push_back(mesh); }
        void clear() { vertexes.clear(); meshes.clear(); }
        
    } canvas;

    void resetCamera();

    class Camera : public ofEasyCam
    {
        public:
        glm::vec3 speed;
        float focal, extrusion;
        bool orbit;
        
    } camera;

    ofImage image, image_depth;
    int example, nPlanes;

    bool bHelp = true, bLoaded = false, bDepth = false, bPlanes = false;

    ofColor central_color, edge_color;

#ifdef LEAP_MOTION_ON
    size_t hand_id, finger_id;
    ofxLeapMotion leap;
    vector<ofxLeapMotionSimpleHand> hands;
#endif
};
