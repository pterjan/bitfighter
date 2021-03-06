//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _GEOMOBJECT_H_
#define _GEOMOBJECT_H_

#include "Geometry_Base.h"    // For Geometry class def

#include "Point.h"
#include "tnlVector.h"

#include <string>

using namespace TNL;

namespace Zap
{

class GeometryContainer
{
   friend class GeomObject;

private:
   Geometry *mGeometry;
   void setGeometry(Geometry *geometry);

public:
   GeometryContainer();                                     // Constructor
   GeometryContainer(const GeometryContainer &container);   // Copy constructor
   virtual ~GeometryContainer();                            // Destructor

   Geometry *getGeometry() const;
   void setGeometry(const Vector<Point> &points) const;

   void reverseWinding() const;

   const Vector<Point> *getOutline() const;
   const Vector<Point> *getFill() const;
   
   Point getVert(S32 index) const;
   string geomToLevelCode() const;
};


////////////////////////////////////////
////////////////////////////////////////

class GeomObject 
{
private:
   GeometryContainer mGeometry;

public:
   GeomObject();                          // Constructor
   virtual ~GeomObject();                 // Destructor

   void setNewGeometry(GeomType geomType, F32 radius = 0);

   GeomType getGeomType() const;

   virtual Point getVert(S32 index) const;               // Overridden by MoveObject
   virtual void setVert(const Point &pos, S32 index);    // Overridden by MoveObject

   S32 getMinVertCount() const;       // Minimum  vertices geometry needs to be viable
   S32 getVertCount() const;          // Actual number of vertices in the geometry

   bool anyVertsSelected() const;
   void selectVert(S32 vertIndex) const;
   void aselectVert(S32 vertIndex) const;   // Select another vertex (remember cmdline ArcInfo?)
   void unselectVert(S32 vertIndex) const;

   void clearVerts();
   bool addVert(const Point &point, bool ignoreMaxPointsLimit = false);
   bool addVertFront(Point vert);
   bool deleteVert(S32 vertIndex);
   bool insertVert(Point vertex, S32 vertIndex);
   void unselectVerts() const;
   bool vertSelected(S32 vertIndex) const;

   // Transforming the geometry
   void rotateAboutPoint(const Point &center, F32 angle) const;
   void flip(F32 center, bool isHoriz) const;               // Do a horizontal or vertical flip about line at center
   void scale(const Point &center, F32 scale) const;
   void offset(const Point &offset) const;                  // Offset object by a certain amount

   // Move object to location, specifying (optional) vertex to be positioned at pos
   virtual void moveTo(const Point &pos, S32 snapVertex = 0);  
   

   // Getting parts of the geometry
   Point getCentroid() const;
   Point getLiveCentroid() const;
   F32 getLabelAngle() const;

   virtual const Vector<Point> *getOutline() const;
           const Vector<Point> *getFill()    const;

   void reverseWinding() const;     

   virtual Rect calcExtents() const;
   bool hasGeometry() const;

   void setGeometry(const Vector<Point> &points) const;

   void disableTriangulation() const;

   // Sending/receiving
   void packGeom(GhostConnection *connection, BitStream *stream) const;
   void unpackGeom(GhostConnection *connection, BitStream *stream);

   // Saving/loading
   string geomToLevelCode() const;
   void readGeom(S32 argc, const char **argv, S32 firstCoord, F32 gridSize);

   GeometryContainer &getGeometry();

   virtual void onGeomChanging();      // Item geom is interactively changing
   virtual void onGeomChanged();       // Item changed geometry (or moved), do any internal updating that might be required

   virtual void setExtent(const Rect &extentRect) = 0;

   virtual Point getPos() const;
   virtual Point getRenderPos() const;

   virtual void setPos(const Point &pos);

   virtual void setGeom(const Vector<Point> &points);

   //friend class ObjectTest;
   //FRIEND_TEST(ObjectTest, GhostingSanity);
};

}


#endif
