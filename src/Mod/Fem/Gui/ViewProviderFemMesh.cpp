/***************************************************************************
 *   Copyright (c) 2013 J�rgen Riegel (FreeCAD@juergen-riegel.net)         *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
# include <Standard_math.hxx>
# include <Inventor/SoDB.h>
# include <Inventor/SoInput.h>
# include <Inventor/SbVec3f.h>
# include <Inventor/actions/SoSearchAction.h>
# include <Inventor/nodes/SoBaseColor.h>
# include <Inventor/nodes/SoLightModel.h>
# include <Inventor/nodes/SoMaterial.h>
# include <Inventor/nodes/SoSeparator.h>
# include <Inventor/nodes/SoTransform.h>
# include <Inventor/nodes/SoRotation.h>
# include <Inventor/nodes/SoCoordinate3.h>
# include <Inventor/nodes/SoDrawStyle.h>
# include <Inventor/nodes/SoIndexedFaceSet.h>
# include <Inventor/nodes/SoIndexedLineSet.h>
# include <Inventor/nodes/SoShapeHints.h>
# include <Inventor/nodes/SoAnnotation.h>
# include <Inventor/nodes/SoPointSet.h>
# include <Inventor/nodes/SoPolygonOffset.h>
# include <Inventor/SoPickedPoint.h>
# include <Inventor/details/SoFaceDetail.h>
# include <Inventor/details/SoLineDetail.h>
# include <Inventor/details/SoPointDetail.h>
# include <QFile>
#endif

#include "ViewProviderFemMesh.h"
#include "ViewProviderFemMeshPy.h"

#include <Mod/Fem/App/FemMeshObject.h>
#include <Mod/Fem/App/FemMesh.h>
#include <App/Document.h>
#include <Base/FileInfo.h>
#include <Base/Stream.h>
#include <Base/Console.h>
#include <Base/TimeInfo.h>
#include <Base/BoundBox.h>
#include <sstream>

#include <SMESH_Mesh.hxx>
#include <SMESHDS_Mesh.hxx>
#include <SMDSAbs_ElementType.hxx>

using namespace FemGui;






struct FemFace 
{
	const SMDS_MeshNode *Nodes[8];
	unsigned long  ElementNumber; 
    const SMDS_MeshElement* Element;
	unsigned short Size;
	unsigned short FaceNo;
    bool hide;
    Base::Vector3d getFirstNodePoint(void) {
        return Base::Vector3d(Nodes[0]->X(),Nodes[0]->Y(),Nodes[0]->Z());
    }
	
	Base::Vector3d set(short size,const SMDS_MeshElement* element,unsigned short id, short faceNo, const SMDS_MeshNode* n1,const SMDS_MeshNode* n2,const SMDS_MeshNode* n3,const SMDS_MeshNode* n4=0,const SMDS_MeshNode* n5=0,const SMDS_MeshNode* n6=0,const SMDS_MeshNode* n7=0,const SMDS_MeshNode* n8=0);
	
	bool isSameFace (FemFace &face);
};

Base::Vector3d FemFace::set(short size,const SMDS_MeshElement* element,unsigned short id,short faceNo, const SMDS_MeshNode* n1,const SMDS_MeshNode* n2,const SMDS_MeshNode* n3,const SMDS_MeshNode* n4,const SMDS_MeshNode* n5,const SMDS_MeshNode* n6,const SMDS_MeshNode* n7,const SMDS_MeshNode* n8)
{
	Nodes[0] = n1;
	Nodes[1] = n2;
	Nodes[2] = n3;
	Nodes[3] = n4;
	Nodes[4] = n5;
	Nodes[5] = n6;
	Nodes[6] = n7;
	Nodes[7] = n8;

    Element         = element;
    ElementNumber   = id; 
    Size            = size;
    FaceNo          = faceNo;
    hide            = false;

	// sorting the nodes for later easier comparison (bubble sort)
    int i, j, flag = 1;    // set flag to 1 to start first pass
	const SMDS_MeshNode* temp;   // holding variable
	
	for(i = 1; (i <= size) && flag; i++)
	{
		flag = 0;
		for (j=0; j < (size -1); j++)
		{
			if (Nodes[j+1] > Nodes[j])      // ascending order simply changes to <
			{ 
				temp = Nodes[j];             // swap elements
				Nodes[j] = Nodes[j+1];
				Nodes[j+1] = temp;
				flag = 1;               // indicates that a swap occurred.
			}
		}
	}

    return Base::Vector3d(Nodes[0]->X(),Nodes[0]->Y(),Nodes[0]->Z());
};

class FemFaceGridItem :public std::vector<FemFace*>{
public:
    //FemFaceGridItem(void){reserve(200);}
};

bool FemFace::isSameFace (FemFace &face) 
{
    // the same element can not have the same face
    if(face.ElementNumber == ElementNumber)
        return false;
	if(face.Size != Size)
        return false;
	// if the same face size just compare if the sorted nodes are the same
	if( Nodes[0] == face.Nodes[0] &&
	    Nodes[1] == face.Nodes[1] &&
		Nodes[2] == face.Nodes[2] &&
		Nodes[3] == face.Nodes[3] &&
		Nodes[4] == face.Nodes[4] &&
		Nodes[5] == face.Nodes[5] &&
		Nodes[6] == face.Nodes[6] &&
        Nodes[7] == face.Nodes[7] ){
            hide = true;
            face.hide = true;
            return true;
    }

    return false;
};

PROPERTY_SOURCE(FemGui::ViewProviderFemMesh, Gui::ViewProviderGeometryObject)

App::PropertyFloatConstraint::Constraints ViewProviderFemMesh::floatRange = {1.0,64.0,1.0};

ViewProviderFemMesh::ViewProviderFemMesh()
{
    sPixmap = "fem-fem-mesh-from-shape";

    ADD_PROPERTY(PointColor,(App::Color(0.7f,0.7f,0.7f)));
    ADD_PROPERTY(PointSize,(5.0f));
    PointSize.setConstraints(&floatRange);
    ADD_PROPERTY(LineWidth,(2.0f));
    LineWidth.setConstraints(&floatRange);

    ShapeColor.setValue(App::Color(1.0f,0.7f,0.0f));
    ADD_PROPERTY(BackfaceCulling,(true));
    ADD_PROPERTY(ShowInner, (false));

    onlyEdges = false;

    pcDrawStyle = new SoDrawStyle();
    pcDrawStyle->ref();
    pcDrawStyle->style = SoDrawStyle::LINES;
    pcDrawStyle->lineWidth = LineWidth.getValue();

    pShapeHints = new SoShapeHints;
    pShapeHints->shapeType = SoShapeHints::SOLID;
    pShapeHints->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
    pShapeHints->ref();

    pcMatBinding = new SoMaterialBinding;
    pcMatBinding->value = SoMaterialBinding::OVERALL;
    pcMatBinding->ref();

    pcCoords = new SoCoordinate3();
    pcCoords->ref();

    pcAnoCoords = new SoCoordinate3();
    pcAnoCoords->ref();
    pcAnoCoords->point.setNum(0);

    pcFaces = new SoIndexedFaceSet;
    pcFaces->ref();

    pcLines = new SoIndexedLineSet;
    pcLines->ref();

    pcPointStyle = new SoDrawStyle();
    pcPointStyle->ref();
    pcPointStyle->style = SoDrawStyle::POINTS;
    pcPointStyle->pointSize = PointSize.getValue();

    pcPointMaterial = new SoMaterial;
    pcPointMaterial->ref();
    //PointMaterial.touch();

    DisplacementFactor = 0;
}

ViewProviderFemMesh::~ViewProviderFemMesh()
{
    pcCoords->unref();
    pcDrawStyle->unref();
    pcFaces->unref();
    pcLines->unref();
    pShapeHints->unref();
    pcMatBinding->unref();
    pcPointMaterial->unref();
    pcPointStyle->unref();

}

void ViewProviderFemMesh::attach(App::DocumentObject *pcObj)
{
    ViewProviderGeometryObject::attach(pcObj);

    // Move 'coords' before the switch
    //pcRoot->insertChild(pcCoords,pcRoot->findChild(reinterpret_cast<const SoNode*>(pcModeSwitch)));

    // Annotation sets
    SoGroup* pcAnotRoot = new SoAnnotation();

    SoDrawStyle *pcAnoStyle = new SoDrawStyle();
    pcAnoStyle->style = SoDrawStyle::POINTS;
    pcAnoStyle->pointSize = 5;

    SoMaterial * pcAnoMaterial = new SoMaterial;
    pcAnoMaterial->diffuseColor.setValue(0,1,0);
    pcAnoMaterial->emissiveColor.setValue(0,1,0);
    pcAnotRoot->addChild(pcAnoMaterial);  
    pcAnotRoot->addChild(pcAnoStyle);
    pcAnotRoot->addChild(pcAnoCoords);
    SoPointSet * pointset = new SoPointSet;
    pcAnotRoot->addChild(pointset);

    // flat
    SoGroup* pcFlatRoot = new SoGroup();
    // face nodes
    pcFlatRoot->addChild(pcCoords);
    pcFlatRoot->addChild(pShapeHints);
    pcFlatRoot->addChild(pcShapeMaterial);
    pcFlatRoot->addChild(pcMatBinding);
    pcFlatRoot->addChild(pcFaces);
    pcFlatRoot->addChild(pcAnotRoot);
    addDisplayMaskMode(pcFlatRoot, "Flat");

    // line
    SoLightModel* pcLightModel = new SoLightModel();
    pcLightModel->model = SoLightModel::BASE_COLOR;
    SoGroup* pcWireRoot = new SoGroup();
    pcWireRoot->addChild(pcCoords);
    pcWireRoot->addChild(pcDrawStyle);
    pcWireRoot->addChild(pcLightModel);
    SoBaseColor* color = new SoBaseColor();
    color->rgb.setValue(0.0f,0.0f,0.0f);
    pcWireRoot->addChild(color);
    pcWireRoot->addChild(pcLines);
    addDisplayMaskMode(pcWireRoot, "Wireframe");


    // Points
    SoGroup* pcPointsRoot = new SoSeparator();
    pcPointsRoot->addChild(pcPointMaterial);  
    pcPointsRoot->addChild(pcPointStyle);
    pcPointsRoot->addChild(pcCoords);
    pointset = new SoPointSet;
    pcPointsRoot->addChild(pointset);
    addDisplayMaskMode(pcPointsRoot, "Nodes");

    // flat+line (Elements)
    SoPolygonOffset* offset = new SoPolygonOffset();
    offset->styles = SoPolygonOffset::LINES;
    //offset->factor = 2.0f;
    //offset->units = 1.0f;
    SoGroup* pcFlatWireRoot = new SoSeparator();
    // add the complete flat group (contains the coordinates)
    pcFlatWireRoot->addChild(pcFlatRoot);
    //pcFlatWireRoot->addChild(offset); // makes no differents.....
    // add the line nodes
    SoMaterialBinding *pcMatBind = new SoMaterialBinding;
    pcMatBind->value = SoMaterialBinding::OVERALL;
    pcFlatWireRoot->addChild(pcMatBind);
    pcFlatWireRoot->addChild(pcDrawStyle);
    pcFlatWireRoot->addChild(pcLightModel);
    pcFlatWireRoot->addChild(color);
    pcFlatWireRoot->addChild(pcLines);

    addDisplayMaskMode(pcFlatWireRoot, "Elements");

    // flat+line+Nodes (Elements&Nodes)
    SoGroup* pcElemNodesRoot = new SoSeparator();
    // add the complete flat group (contains the coordinates)
    pcElemNodesRoot->addChild(pcFlatRoot);
    //pcElemNodesRoot->addChild(offset);
    // add the line nodes
    pcElemNodesRoot->addChild(pcDrawStyle);
    pcElemNodesRoot->addChild(pcLightModel);
    pcElemNodesRoot->addChild(color);
    pcElemNodesRoot->addChild(pcLines);
    // add the points nodes
    pcElemNodesRoot->addChild(pcPointMaterial);
    pcElemNodesRoot->addChild(pcPointStyle);
    pcElemNodesRoot->addChild(pcPointMaterial);
    pcElemNodesRoot->addChild(pointset);

    addDisplayMaskMode(pcElemNodesRoot, "Elements & Nodes");



}

void ViewProviderFemMesh::setDisplayMode(const char* ModeName)
{
    if (strcmp("Elements",ModeName)==0)
        setDisplayMaskMode("Elements");
    else if (strcmp("Elements & Nodes",ModeName)==0)
        setDisplayMaskMode("Elements & Nodes");
    else if (strcmp("Flat",ModeName)==0)
        setDisplayMaskMode("Flat");
    else if (strcmp("Wireframe",ModeName)==0)
        setDisplayMaskMode("Wireframe");
    else if (strcmp("Nodes",ModeName)==0)
        setDisplayMaskMode("Nodes");

    ViewProviderGeometryObject::setDisplayMode( ModeName );
}

std::vector<std::string> ViewProviderFemMesh::getDisplayModes(void) const
{
    std::vector<std::string> StrList;
    StrList.push_back("Elements");
    StrList.push_back("Elements & Nodes");
    StrList.push_back("Flat");
    StrList.push_back("Wireframe");
    StrList.push_back("Nodes");
    return StrList;
}

void ViewProviderFemMesh::updateData(const App::Property* prop)
{
    if (prop->isDerivedFrom(Fem::PropertyFemMesh::getClassTypeId())) {
        ViewProviderFEMMeshBuilder builder;
        resetColorByNodeId();
        resetDisplacementByNodeId();
        builder.createMesh(prop, pcCoords, pcFaces, pcLines, vFaceElementIdx, vNodeElementIdx, onlyEdges, ShowInner.getValue());
    }
    Gui::ViewProviderGeometryObject::updateData(prop);
}

void ViewProviderFemMesh::onChanged(const App::Property* prop)
{
    if (prop == &PointSize) {
        pcPointStyle->pointSize = PointSize.getValue();
    }
    else if (prop == &PointColor) {
        const App::Color& c = PointColor.getValue();
        pcPointMaterial->diffuseColor.setValue(c.r,c.g,c.b);
    }
    else if (prop == &BackfaceCulling) {
        if(BackfaceCulling.getValue()){
            pShapeHints->shapeType = SoShapeHints::SOLID;
            //pShapeHints->vertexOrdering = SoShapeHints::CLOCKWISE;
        }else{
            pShapeHints->shapeType = SoShapeHints::UNKNOWN_SHAPE_TYPE;
            //pShapeHints->vertexOrdering = SoShapeHints::CLOCKWISE;
        }
    }
    else if (prop == &ShowInner ) {
        // recalc mesh with new settings
        ViewProviderFEMMeshBuilder builder;
        builder.createMesh(&(dynamic_cast<Fem::FemMeshObject*>(this->pcObject)->FemMesh), pcCoords, pcFaces, pcLines, vFaceElementIdx, vNodeElementIdx, onlyEdges, ShowInner.getValue());
    }
    else if (prop == &LineWidth) {
        pcDrawStyle->lineWidth = LineWidth.getValue();
    }
    else {
        ViewProviderGeometryObject::onChanged(prop);
    }
}

std::string ViewProviderFemMesh::getElement(const SoDetail* detail) const
{
    std::stringstream str;
    if (detail) {
        if (detail->getTypeId() == SoFaceDetail::getClassTypeId()) {
            const SoFaceDetail* face_detail = static_cast<const SoFaceDetail*>(detail);
            unsigned long edx = vFaceElementIdx[face_detail->getFaceIndex()];

            str << "Elem" << (edx>>3) << "F"<< (edx&7)+1;
        }
        // trigger on edges only if edge only mesh, otherwise you only hit edges an never faces....
        else if (onlyEdges && detail->getTypeId() == SoLineDetail::getClassTypeId()) {
            const SoLineDetail* line_detail = static_cast<const SoLineDetail*>(detail);
            int edge = line_detail->getLineIndex() + 1;
            str << "Edge" << edge;
        }
        else if (detail->getTypeId() == SoPointDetail::getClassTypeId()) {
            const SoPointDetail* point_detail = static_cast<const SoPointDetail*>(detail);
            int idx = point_detail->getCoordinateIndex();
            if (idx < vNodeElementIdx.size()) {
                int vertex = vNodeElementIdx[point_detail->getCoordinateIndex()];
                str << "Node" << vertex;
            }
            else {
                return std::string();
            }
        }
    }

    return str.str();
}

SoDetail* ViewProviderFemMesh::getDetail(const char* subelement) const
{
    std::string element = subelement;
    std::string::size_type pos = element.find_first_of("0123456789");
    int index = -1;
    if (pos != std::string::npos) {
        index = std::atoi(element.substr(pos).c_str());
        element = element.substr(0,pos);
    }

    SoDetail* detail = 0;
    if (index < 0)
        return detail;
    if (element == "Elem") {
        detail = new SoFaceDetail();
        static_cast<SoFaceDetail*>(detail)->setPartIndex(index - 1);
    }
    //else if (element == "Edge") {
    //    detail = new SoLineDetail();
    //    static_cast<SoLineDetail*>(detail)->setLineIndex(index - 1);
    //}
    //else if (element == "Vertex") {
    //    detail = new SoPointDetail();
    //    static_cast<SoPointDetail*>(detail)->setCoordinateIndex(index + nodeset->startIndex.getValue() - 1);
    //}

    return detail;
}

std::vector<Base::Vector3d> ViewProviderFemMesh::getSelectionShape(const char* Element) const
{
    return std::vector<Base::Vector3d>();
}

void ViewProviderFemMesh::setHighlightNodes(const std::set<long>& HighlightedNodes)
{
    if(!HighlightedNodes.empty()){
        SMESHDS_Mesh* data = const_cast<SMESH_Mesh*>((dynamic_cast<Fem::FemMeshObject*>(this->pcObject)->FemMesh).getValue().getSMesh())->GetMeshDS();

        pcAnoCoords->point.setNum(HighlightedNodes.size());
        SbVec3f* verts = pcAnoCoords->point.startEditing();
        int i=0;
        for(std::set<long>::const_iterator it=HighlightedNodes.begin();it!=HighlightedNodes.end();++it,i++){
            const SMDS_MeshNode *Node = data->FindNode(*it);
            if (Node)
                verts[i].setValue((float)Node->X(),(float)Node->Y(),(float)Node->Z());
            else
                verts[i].setValue(0,0,0);
        }
        pcAnoCoords->point.finishEditing();
    }else{
        pcAnoCoords->point.setNum(0);
    }
}
void ViewProviderFemMesh::resetHighlightNodes(void)
{
    pcAnoCoords->point.setNum(0);
}

PyObject * ViewProviderFemMesh::getPyObject()
{
    if (PythonObject.is(Py::_None())){
        // ref counter is set to 1
        PythonObject = Py::Object(new ViewProviderFemMeshPy(this),true);
    }
    return Py::new_reference_to(PythonObject); 
}

void ViewProviderFemMesh::setColorByNodeId(const std::map<long,App::Color> &NodeColorMap)
{
    long startId = NodeColorMap.begin()->first;
    long endId = (--NodeColorMap.end())->first;

    std::vector<App::Color> colorVec(endId-startId+2,App::Color(0,1,0));
    for(std::map<long,App::Color>::const_iterator it=NodeColorMap.begin();it!=NodeColorMap.end();++it)
        colorVec[it->first-startId] = it->second;

    setColorByNodeIdHelper(colorVec);

}
void ViewProviderFemMesh::setColorByNodeId(const std::vector<long> &NodeIds,const std::vector<App::Color> &NodeColors)
{

    long startId = *(std::min_element(NodeIds.begin(), NodeIds.end()));     
    long endId   = *(std::max_element(NodeIds.begin(), NodeIds.end()));

    std::vector<App::Color> colorVec(endId-startId+2,App::Color(0,1,0));
    long i=0;
    for(std::vector<long>::const_iterator it=NodeIds.begin();it!=NodeIds.end();++it,i++)
        colorVec[*it-startId] = NodeColors[i];


    setColorByNodeIdHelper(colorVec);

}

void ViewProviderFemMesh::setColorByNodeIdHelper(const std::vector<App::Color> &colorVec)
{
    pcMatBinding->value = SoMaterialBinding::PER_VERTEX_INDEXED;

    // resizing and writing the color vector:
    pcShapeMaterial->diffuseColor.setNum(vNodeElementIdx.size());
    SbColor* colors = pcShapeMaterial->diffuseColor.startEditing();

    long i=0;
    for(std::vector<unsigned long>::const_iterator it=vNodeElementIdx.begin()
            ;it!=vNodeElementIdx.end()
            ;++it,i++)
       colors[i] = SbColor(colorVec[*it-1].r,colorVec[*it-1].g,colorVec[*it-1].b);


    pcShapeMaterial->diffuseColor.finishEditing();
}

void ViewProviderFemMesh::resetColorByNodeId(void)
{
    pcMatBinding->value = SoMaterialBinding::OVERALL;
    pcShapeMaterial->diffuseColor.setNum(0);
    const App::Color& c = ShapeColor.getValue();
    pcShapeMaterial->diffuseColor.setValue(c.r,c.g,c.b);

}

void ViewProviderFemMesh::setDisplacementByNodeId(const std::map<long,Base::Vector3d> &NodeDispMap)
{
    long startId = NodeDispMap.begin()->first;
    long endId = (--NodeDispMap.end())->first;

    std::vector<Base::Vector3d> vecVec(endId-startId+2,Base::Vector3d());

    for(std::map<long,Base::Vector3d>::const_iterator it=NodeDispMap.begin();it!=NodeDispMap.end();++it)
        vecVec[it->first-startId] = it->second;

    setDisplacementByNodeIdHelper(vecVec,startId);
}

void ViewProviderFemMesh::setDisplacementByNodeId(const std::vector<long> &NodeIds,const std::vector<Base::Vector3d> &NodeDisps)
{
    long startId = *(std::min_element(NodeIds.begin(), NodeIds.end()));     
    long endId   = *(std::max_element(NodeIds.begin(), NodeIds.end()));

    std::vector<Base::Vector3d> vecVec(endId-startId+2,Base::Vector3d());

    long i=0;
    for(std::vector<long>::const_iterator it=NodeIds.begin();it!=NodeIds.end();++it,i++)
        vecVec[*it-startId] = NodeDisps[i];

    setDisplacementByNodeIdHelper(vecVec,startId);
}

void ViewProviderFemMesh::setDisplacementByNodeIdHelper(const std::vector<Base::Vector3d>& DispVector,long startId)
{
    DisplacementVector.resize(vNodeElementIdx.size());
    int i=0;
    for(std::vector<unsigned long>::const_iterator it=vNodeElementIdx.begin();it!=vNodeElementIdx.end();++it,i++)
        DisplacementVector[i] = DispVector[*it-startId];
    applyDisplacementToNodes(1.0);

}

void ViewProviderFemMesh::resetDisplacementByNodeId(void)
{
    applyDisplacementToNodes(0.0);
    DisplacementVector.clear();
}
/// reaply the node displacement with a certain factor and do a redraw
void ViewProviderFemMesh::applyDisplacementToNodes(double factor)
{
    if(DisplacementVector.size() == 0)
        return;

    float x,y,z;
    // set the point coordinates
    long sz = pcCoords->point.getNum();
    SbVec3f* verts = pcCoords->point.startEditing();
    for (long i=0;i < sz ;i++) {
        verts[i].getValue(x,y,z);
        // undo old factor#
        Base::Vector3d oldDisp = DisplacementVector[i] * DisplacementFactor;
        x -= oldDisp.x;
        y -= oldDisp.y;
        z -= oldDisp.z;
        // apply new factor
        Base::Vector3d newDisp = DisplacementVector[i] * factor;
        x += newDisp.x;
        y += newDisp.y;
        z += newDisp.z;
        // set the new value
        verts[i].setValue(x,y,z);
    }
    pcCoords->point.finishEditing();

    DisplacementFactor = factor;
}

void ViewProviderFemMesh::setColorByElementId(const std::map<long,App::Color> &ElementColorMap)
{
    pcMatBinding->value = SoMaterialBinding::PER_FACE ;

    // resizing and writing the color vector:
    pcShapeMaterial->diffuseColor.setNum(vFaceElementIdx.size());
    SbColor* colors = pcShapeMaterial->diffuseColor.startEditing();

    int i=0;
    for(std::vector<unsigned long>::const_iterator it=vFaceElementIdx.begin()
            ;it!=vFaceElementIdx.end()
            ;++it,i++){
        unsigned long ElemIdx = ((*it)>>3);
        const std::map<long,App::Color>::const_iterator pos = ElementColorMap.find(ElemIdx);
        if(pos == ElementColorMap.end())
            colors[i] = SbColor(0,1,0);
        else
            colors[i] = SbColor(pos->second.r,pos->second.g,pos->second.b);
    }

    pcShapeMaterial->diffuseColor.finishEditing();
}

void ViewProviderFemMesh::resetColorByElementId(void)
{
    pcMatBinding->value = SoMaterialBinding::OVERALL;
    pcShapeMaterial->diffuseColor.setNum(0);
    const App::Color& c = ShapeColor.getValue();
    pcShapeMaterial->diffuseColor.setValue(c.r,c.g,c.b);

}

// ----------------------------------------------------------------------------

void ViewProviderFEMMeshBuilder::buildNodes(const App::Property* prop, std::vector<SoNode*>& nodes) const
{
    SoCoordinate3 *pcPointsCoord=0;
    SoIndexedFaceSet *pcFaces=0;
    SoIndexedLineSet *pcLines=0;

    if (nodes.empty()) {
        pcPointsCoord = new SoCoordinate3();
        nodes.push_back(pcPointsCoord);
        pcFaces = new SoIndexedFaceSet();
        pcLines = new SoIndexedLineSet();
        nodes.push_back(pcFaces);
    }
    else if (nodes.size() == 2) {
        if (nodes[0]->getTypeId() == SoCoordinate3::getClassTypeId())
            pcPointsCoord = static_cast<SoCoordinate3*>(nodes[0]);
        if (nodes[1]->getTypeId() == SoIndexedFaceSet::getClassTypeId())
            pcFaces = static_cast<SoIndexedFaceSet*>(nodes[1]);
    }

    if (pcPointsCoord && pcFaces){
        std::vector<unsigned long> vFaceElementIdx;
        std::vector<unsigned long> vNodeElementIdx;
        bool onlyEdges;
        createMesh(prop, pcPointsCoord, pcFaces,pcLines,vFaceElementIdx,vNodeElementIdx,onlyEdges,false);
    }
}

inline void insEdgeVec(std::map<int,std::set<int> > &map, int n1, int n2)
{
    //FIXME: The if-else distinction doesn't make sense
    //if (n1<n2)
    //    map[n2].insert(n1);
    //else
        map[n2].insert(n1);
};

inline unsigned long ElemFold(unsigned long Element,unsigned long FaceNbr)
{
    unsigned long t1 = Element<<3;
    unsigned long t2 = t1 | FaceNbr;
    return  t2;
}

void ViewProviderFEMMeshBuilder::createMesh(const App::Property* prop, 
                                            SoCoordinate3* coords, 
                                            SoIndexedFaceSet* faces,
                                            SoIndexedLineSet* lines, 
                                            std::vector<unsigned long> &vFaceElementIdx, 
                                            std::vector<unsigned long> &vNodeElementIdx, 
                                            bool &onlyEdges,
                                            bool ShowInner) const
{

    const Fem::PropertyFemMesh* mesh = static_cast<const Fem::PropertyFemMesh*>(prop);

    SMESHDS_Mesh* data = const_cast<SMESH_Mesh*>(mesh->getValue().getSMesh())->GetMeshDS();

	int numFaces = data->NbFaces();
	int numNodes = data->NbNodes();
	int numEdges = data->NbEdges();
	
    if(numFaces+numNodes+numEdges == 0) return;
    Base::TimeInfo Start;
    Base::Console().Log("Start: ViewProviderFEMMeshBuilder::createMesh() =================================\n");

	const SMDS_MeshInfo& info = data->GetMeshInfo();
    int numTria = info.NbTriangles();
    int numQuad = info.NbQuadrangles();
    int numPoly = info.NbPolygons();
                                                
    int numVolu = info.NbVolumes();
    int numTetr = info.NbTetras();
    int numHexa = info.NbHexas();
    int numPyrd = info.NbPyramids();
    int numPris = info.NbPrisms();
    int numHedr = info.NbPolyhedrons();


    bool ShowFaces = (numFaces >0 && numVolu == 0);

    int numTries;
    if(ShowFaces)
        numTries = numTria+numQuad/*+numPoly*/+numTetr*4+numHexa*6+numPyrd*5+numPris*6;
    else
        numTries = numTetr*4+numHexa*6+numPyrd*5+numPris*6;

    // corner case only edges (Beams) in the mesh. This need some special cases in building up visual
    onlyEdges = false;
    if (numFaces <= 0 && numVolu <= 0 && numEdges > 0){
        onlyEdges = true;
    }
        
    std::vector<FemFace> facesHelper(numTries);

    Base::Console().Log("    %f: Start build up %i face helper\n",Base::TimeInfo::diffTimeF(Start,Base::TimeInfo()),facesHelper.size());
    Base::BoundBox3d BndBox;

    int i=0;

    if (ShowFaces){
        SMDS_FaceIteratorPtr aFaceIter = data->facesIterator();
        for (;aFaceIter->more();) {
            const SMDS_MeshFace* aFace = aFaceIter->next();

            int num = aFace->NbNodes();
            switch(num){
                
            case 4:// quad face
                BndBox.Add(facesHelper[i++].set(4, aFace, aFace->GetID(), 0, aFace->GetNode(0), aFace->GetNode(1), aFace->GetNode(2), aFace->GetNode(3)));
                break;
            case 3:// tria face
                BndBox.Add(facesHelper[i++].set(3, aFace, aFace->GetID(), 0, aFace->GetNode(0), aFace->GetNode(1), aFace->GetNode(2)));
                break;
            case 6:// tria face with 6 nodes
                BndBox.Add(facesHelper[i++].set(6, aFace, aFace->GetID(), 0, aFace->GetNode(0), aFace->GetNode(1), aFace->GetNode(2), aFace->GetNode(3), aFace->GetNode(4), aFace->GetNode(5)));
                break;

                //unknown case
                default: assert(0);
            }
        }
    }
    else{

        // iterate all volumes
        SMDS_VolumeIteratorPtr aVolIter = data->volumesIterator();
        for (; aVolIter->more();) {
            const SMDS_MeshVolume* aVol = aVolIter->next();

            int num = aVol->NbNodes();

            switch (num){
                // tet 4 element 
            case 4:
                // face 1
                BndBox.Add(facesHelper[i++].set(3, aVol, aVol->GetID(), 1, aVol->GetNode(0), aVol->GetNode(1), aVol->GetNode(2)));
                // face 2
                BndBox.Add(facesHelper[i++].set(3, aVol, aVol->GetID(), 2, aVol->GetNode(0), aVol->GetNode(3), aVol->GetNode(1)));
                // face 3
                BndBox.Add(facesHelper[i++].set(3, aVol, aVol->GetID(), 3, aVol->GetNode(1), aVol->GetNode(3), aVol->GetNode(2)));
                // face 4
                BndBox.Add(facesHelper[i++].set(3, aVol, aVol->GetID(), 4, aVol->GetNode(2), aVol->GetNode(3), aVol->GetNode(0)));
                break;
                //unknown case
            case 8:
                // face 1
                BndBox.Add(facesHelper[i++].set(4, aVol, aVol->GetID(), 1, aVol->GetNode(0), aVol->GetNode(1), aVol->GetNode(2), aVol->GetNode(3)));
                // face 2
                BndBox.Add(facesHelper[i++].set(4, aVol, aVol->GetID(), 2, aVol->GetNode(4), aVol->GetNode(5), aVol->GetNode(6), aVol->GetNode(7)));
                // face 3
                BndBox.Add(facesHelper[i++].set(4, aVol, aVol->GetID(), 3, aVol->GetNode(0), aVol->GetNode(1), aVol->GetNode(4), aVol->GetNode(5)));
                // face 4
                BndBox.Add(facesHelper[i++].set(4, aVol, aVol->GetID(), 4, aVol->GetNode(1), aVol->GetNode(2), aVol->GetNode(5), aVol->GetNode(6)));
                // face 5
                BndBox.Add(facesHelper[i++].set(4, aVol, aVol->GetID(), 5, aVol->GetNode(2), aVol->GetNode(3), aVol->GetNode(6), aVol->GetNode(7)));
                // face 6
                BndBox.Add(facesHelper[i++].set(4, aVol, aVol->GetID(), 6, aVol->GetNode(0), aVol->GetNode(3), aVol->GetNode(4), aVol->GetNode(7)));
                break;
                //unknown case
            case 10:
                // face 1
                BndBox.Add(facesHelper[i++].set(6, aVol, aVol->GetID(), 1, aVol->GetNode(0), aVol->GetNode(1), aVol->GetNode(2), aVol->GetNode(4), aVol->GetNode(5), aVol->GetNode(6)));
                // face 2
                BndBox.Add(facesHelper[i++].set(6, aVol, aVol->GetID(), 2, aVol->GetNode(0), aVol->GetNode(3), aVol->GetNode(1), aVol->GetNode(7), aVol->GetNode(8), aVol->GetNode(4)));
                // face 3
                BndBox.Add(facesHelper[i++].set(6, aVol, aVol->GetID(), 3, aVol->GetNode(1), aVol->GetNode(3), aVol->GetNode(2), aVol->GetNode(8), aVol->GetNode(9), aVol->GetNode(5)));
                // face 4
                BndBox.Add(facesHelper[i++].set(6, aVol, aVol->GetID(), 4, aVol->GetNode(2), aVol->GetNode(3), aVol->GetNode(0), aVol->GetNode(9), aVol->GetNode(7), aVol->GetNode(6)));
                break;
                //unknown case
            default: assert(0);
            }
        }
    }
    int FaceSize = facesHelper.size();


    if( FaceSize < 5000){
        Base::Console().Log("    %f: Start eliminate internal faces SIMPLE\n",Base::TimeInfo::diffTimeF(Start,Base::TimeInfo()));
        
        // search for double (inside) faces and hide them
        if(!ShowInner){
            for(int l=0; l< FaceSize;l++){
                if(! facesHelper[l].hide){
                    for(int i=l+1; i<FaceSize; i++){
                        if(facesHelper[l].isSameFace(facesHelper[i]) ){
                            break;
                        }
                    }
                }
            }
        }
    }else{
        Base::Console().Log("    %f: Start eliminate internal faces GRID\n",Base::TimeInfo::diffTimeF(Start,Base::TimeInfo()));
        BndBox.Enlarge(BndBox.CalcDiagonalLength()/10000.0);
        // calculate grid properties
        double edge = pow(FaceSize,1.0/3.0);
        double edgeL = BndBox.LengthX() + BndBox.LengthY() + BndBox.LengthZ();
        double gridFactor = 5.0;
        double size = ( edgeL /(3*edge) )*gridFactor;

        unsigned int NbrX = (unsigned int)(BndBox.LengthX()/size)+1;
        unsigned int NbrY = (unsigned int)(BndBox.LengthY()/size)+1;
        unsigned int NbrZ = (unsigned int)(BndBox.LengthZ()/size)+1;
        Base::Console().Log("      Size:F:%f,  X:%i  ,Y:%i  ,Z:%i\n",gridFactor,NbrX,NbrY,NbrZ);

        double Xmin = BndBox.MinX;
        double Ymin = BndBox.MinY;
        double Zmin = BndBox.MinZ;
        double Xln  = BndBox.LengthX() / NbrX;
        double Yln  = BndBox.LengthY() / NbrY;
        double Zln  = BndBox.LengthZ() / NbrZ;

        std::vector<FemFaceGridItem> Grid(NbrX*NbrY*NbrZ);


        unsigned int iX = 0;
        unsigned int iY = 0;
        unsigned int iZ = 0;

        for(int l=0; l< FaceSize;l++){
            Base::Vector3d point(facesHelper[l].getFirstNodePoint());
            double x = (point.x - Xmin) / Xln;
            double y = (point.y - Ymin) / Yln;
            double z = (point.z - Zmin) / Zln;

            iX = x;
            iY = y;
            iZ = z;

            if(iX >= NbrX || iY >= NbrY || iZ >= NbrZ)
                Base::Console().Log("      Outof range!\n");
            
            Grid[iX + iY*NbrX + iZ*NbrX*NbrY].push_back(&facesHelper[l]);
        }

        unsigned int max =0, avg = 0;
        for(std::vector<FemFaceGridItem>::iterator it=Grid.begin();it!=Grid.end();++it){
            for(unsigned int l=0; l< it->size();l++){
                if(! it->operator[](l)->hide){
                    for(unsigned int i=l+1; i<it->size(); i++){
                        if(it->operator[](l)->isSameFace(*(it->operator[](i))) ){
                            break;
                        }
                    }
                }
            }
            if(it->size() > max)max=it->size();
            avg += it->size();
        }
        avg = avg/Grid.size();
         
        Base::Console().Log("      VoxelSize: Max:%i ,Average:%i\n",max,avg);

    } //if( FaceSize < 1000)


    Base::Console().Log("    %f: Start build up node map\n",Base::TimeInfo::diffTimeF(Start,Base::TimeInfo()));

    // sort out double nodes and build up index map
    std::map<const SMDS_MeshNode*, int> mapNodeIndex;

    // handling the corner case beams only, means no faces/triangles only nodes and edges
    if (onlyEdges){

        SMDS_EdgeIteratorPtr aEdgeIte = data->edgesIterator();
        for (; aEdgeIte->more();) {
            const SMDS_MeshEdge* aEdge = aEdgeIte->next();
            int num = aEdge->NbNodes();
            for (int i = 0; i < num; i++) {
                mapNodeIndex[aEdge->GetNode(i)] = 0;

            }
        }
    }else{

        for (int l = 0; l < FaceSize; l++) {
            if (!facesHelper[l].hide) {
                for (int i = 0; i < 8; i++) {
                    if (facesHelper[l].Nodes[i])
                        mapNodeIndex[facesHelper[l].Nodes[i]] = 0;
                    else
                        break;
                }
            }
        }
    }
    Base::Console().Log("    %f: Start set point vector\n",Base::TimeInfo::diffTimeF(Start,Base::TimeInfo()));

    // set the point coordinates
    coords->point.setNum(mapNodeIndex.size());
    vNodeElementIdx.resize(mapNodeIndex.size() );
    std::map<const SMDS_MeshNode*, int>::iterator it=  mapNodeIndex.begin();
    SbVec3f* verts = coords->point.startEditing();
    for (int i=0;it != mapNodeIndex.end() ;++it,i++) {
        verts[i].setValue((float)it->first->X(),(float)it->first->Y(),(float)it->first->Z());
        it->second = i;
        // set selection idx
        vNodeElementIdx[i] = it->first->GetID();
    }
    coords->point.finishEditing();



    // count triangle size
    int triangleCount=0;
    for (int l = 0; l < FaceSize; l++){
        if (!facesHelper[l].hide)
            switch (facesHelper[l].Size){
            case 3:triangleCount++; break;
            case 4:triangleCount += 2; break;
            case 6:triangleCount += 4; break;
            default: assert(0);
        }
    }
    // edge map collect and sort edges of the faces to be shown. 
    std::map<int,std::set<int> > EdgeMap;

    // handling the corner case beams only, means no faces/triangles only nodes and edges
    if (onlyEdges){

        SMDS_EdgeIteratorPtr aEdgeIte = data->edgesIterator();
        for (; aEdgeIte->more();) {
            const SMDS_MeshEdge* aEdge = aEdgeIte->next();
            int num = aEdge->NbNodes();
            switch (num){
            case 2: { // case for Segment element
                int nIdx0 = mapNodeIndex[aEdge->GetNode(0)];
                int nIdx1 = mapNodeIndex[aEdge->GetNode(1)];
                insEdgeVec(EdgeMap, nIdx0, nIdx1);
                break;
            }

            case 3: { // case for Segment element
                int nIdx0 = mapNodeIndex[aEdge->GetNode(0)];
                int nIdx1 = mapNodeIndex[aEdge->GetNode(1)];
                int nIdx2 = mapNodeIndex[aEdge->GetNode(2)];
                insEdgeVec(EdgeMap, nIdx0, nIdx1);
                insEdgeVec(EdgeMap, nIdx1, nIdx2);
                break;
            }
            }
        }
    }

    Base::Console().Log("    %f: Start build up triangle vector\n",Base::TimeInfo::diffTimeF(Start,Base::TimeInfo()));
    // set the triangle face indices
    faces->coordIndex.setNum(4*triangleCount);
    vFaceElementIdx.resize(triangleCount);
    int index=0,indexIdx=0;
    int32_t* indices = faces->coordIndex.startEditing();
	// iterate all element faces, allways assure CLOCKWISE triangle ordering to allow backface culling
    for(int l=0; l< FaceSize;l++){
        if(! facesHelper[l].hide){
            switch( facesHelper[l].Element->NbNodes()){
                case 3: // Face 3
                    switch (facesHelper[l].FaceNo){
                    case 0: { // case for quad faces
                        int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                        int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                        int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                        indices[index++] = nIdx2;
                        indices[index++] = nIdx0;
                        indices[index++] = nIdx1;
                        indices[index++] = SO_END_FACE_INDEX;
                        insEdgeVec(EdgeMap, nIdx0, nIdx1);
                        insEdgeVec(EdgeMap, nIdx1, nIdx2);
                        insEdgeVec(EdgeMap, nIdx2, nIdx0);

                        vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);

                        break;    }
                    default: assert(0);
                    }
                    break;
                case 4: // Tet 4
                    switch (facesHelper[l].FaceNo){
                    case 0: { // case for quad faces
                        int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                        int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                        int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                        int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                        indices[index++] = nIdx2;
                        indices[index++] = nIdx0;
                        indices[index++] = nIdx1;
                        indices[index++] = SO_END_FACE_INDEX;
                        insEdgeVec(EdgeMap, nIdx0, nIdx1);
                        insEdgeVec(EdgeMap, nIdx1, nIdx2);
                        vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                        indices[index++] = nIdx3;
                        indices[index++] = nIdx0;
                        indices[index++] = nIdx2;
                        indices[index++] = SO_END_FACE_INDEX;
                        insEdgeVec(EdgeMap, nIdx2, nIdx3);
                        insEdgeVec(EdgeMap, nIdx3, nIdx0);
                        vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                        break;    }
                    case 1: { // face 1 of Tet10
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            indices[index++] = nIdx2;     
                            indices[index++] = nIdx0;     
                            indices[index++] = nIdx1;     
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx0,nIdx1);
                            insEdgeVec(EdgeMap,nIdx0,nIdx2);
                            insEdgeVec(EdgeMap,nIdx1,nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,0);
                            break;    }
                        case 2: {
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            indices[index++] = nIdx1;   
                            indices[index++] = nIdx0;   
                            indices[index++] = nIdx3;   
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx0,nIdx1);
                            insEdgeVec(EdgeMap,nIdx0,nIdx3);
                            insEdgeVec(EdgeMap,nIdx1,nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,1);
                            break;    }
                        case 3: {
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx1,nIdx2);
                            insEdgeVec(EdgeMap,nIdx1,nIdx3);
                            insEdgeVec(EdgeMap,nIdx2,nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,2);
                            break;    }
                        case 4: {
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx0,nIdx2);
                            insEdgeVec(EdgeMap,nIdx0,nIdx3);
                            insEdgeVec(EdgeMap,nIdx3,nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,3);
                            break;    }
                        default: assert(0);

                    }
                    break;
                case 6: // face 6
                    switch (facesHelper[l].FaceNo){
                    case 0: { // element face number 0
                        // prefeche all node indexes of this face
                        int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                        int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                        int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                        int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                        int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                        int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                        // create triangle number 1 ----------------------------------------------
                        // fill in the node indexes in CLOCKWISE order
                        indices[index++] = nIdx0;
                        indices[index++] = nIdx3;
                        indices[index++] = nIdx5;
                        indices[index++] = SO_END_FACE_INDEX;
                        // add the two edge segments for that triangle
                        insEdgeVec(EdgeMap, nIdx0, nIdx3);
                        insEdgeVec(EdgeMap, nIdx0, nIdx5);
                        // rember the element and face number for that triangle
                        vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                        // create triangle number 2 ----------------------------------------------
                        indices[index++] = nIdx3;
                        indices[index++] = nIdx1;
                        indices[index++] = nIdx4;
                        indices[index++] = SO_END_FACE_INDEX;
                        insEdgeVec(EdgeMap, nIdx3, nIdx1);
                        insEdgeVec(EdgeMap, nIdx1, nIdx4);
                        vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                        // create triangle number 3 ----------------------------------------------
                        indices[index++] = nIdx4;
                        indices[index++] = nIdx2;
                        indices[index++] = nIdx5;
                        indices[index++] = SO_END_FACE_INDEX;
                        insEdgeVec(EdgeMap, nIdx4, nIdx2);
                        insEdgeVec(EdgeMap, nIdx2, nIdx5);
                        vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber, 0);
                        // create triangle number 4 ----------------------------------------------
                        indices[index++] = nIdx5;
                        indices[index++] = nIdx3;
                        indices[index++] = nIdx4;
                        indices[index++] = SO_END_FACE_INDEX;
                        // this triangle has no edge (inner triangle).
                        break;    }
                     break;
                    }
                case 8: // Hex 8
                    switch(facesHelper[l].FaceNo){
                        case 1: {
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx0,nIdx1);
                            insEdgeVec(EdgeMap,nIdx0,nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,0);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx1;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx2,nIdx1);
                            insEdgeVec(EdgeMap,nIdx2,nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,0);
                            break;    }
                        case 2: {
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx4,nIdx5);
                            insEdgeVec(EdgeMap,nIdx4,nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,1);
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx6,nIdx5);
                            insEdgeVec(EdgeMap,nIdx6,nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,1);
                            break;    }
                        case 3: {
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx1,nIdx0);
                            insEdgeVec(EdgeMap,nIdx1,nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,2);
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx4;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx4,nIdx0);
                            insEdgeVec(EdgeMap,nIdx4,nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,2);
                            break;    }
                        case 4: {
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx2;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx1,nIdx5);
                            insEdgeVec(EdgeMap,nIdx1,nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,3);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx6;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx6,nIdx5);
                            insEdgeVec(EdgeMap,nIdx6,nIdx2);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,3);
                            break;    }
                        case 5: {
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx3,nIdx2);
                            insEdgeVec(EdgeMap,nIdx3,nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,4);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx6;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx6,nIdx2);
                            insEdgeVec(EdgeMap,nIdx6,nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,4);
                            break;    }
                        case 6: {
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx4;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx0,nIdx4);
                            insEdgeVec(EdgeMap,nIdx0,nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,5);
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx7,nIdx4);
                            insEdgeVec(EdgeMap,nIdx7,nIdx3);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,5);
                            break;    }
                    }
                    break;
                case 10: // Tet 10
                    switch(facesHelper[l].FaceNo){
                        case 1: { // element face number 1
                            // prefeche all node indexes of this face
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            // create triangle number 1 ----------------------------------------------
                            // fill in the node indexes in CLOCKWISE order
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx4;
                            indices[index++] = SO_END_FACE_INDEX;
                            // add the two edge segments for that triangle
                            insEdgeVec(EdgeMap,nIdx0,nIdx6);
                            insEdgeVec(EdgeMap,nIdx0,nIdx4);
                            // rember the element and face number for that triangle
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,0);
                            // create triangle number 2 ----------------------------------------------
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx2,nIdx6);
                            insEdgeVec(EdgeMap,nIdx2,nIdx5);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,0);
                            // create triangle number 3 ----------------------------------------------
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx4;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx1,nIdx5);
                            insEdgeVec(EdgeMap,nIdx1,nIdx4);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,0);
                            // create triangle number 4 ----------------------------------------------
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx5;
                            indices[index++] = SO_END_FACE_INDEX;
                            // this triangle has no edge (inner triangle).
                            break;    }
                        case 2: {
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx4 = mapNodeIndex[facesHelper[l].Element->GetNode(4)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx8 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx0,nIdx7);
                            insEdgeVec(EdgeMap,nIdx0,nIdx4);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,1);
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx8;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx1,nIdx8);
                            insEdgeVec(EdgeMap,nIdx1,nIdx4);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,1);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx8;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx3,nIdx7);
                            insEdgeVec(EdgeMap,nIdx3,nIdx8);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,1);
                            indices[index++] = nIdx8;
                            indices[index++] = nIdx4;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,1);
                            break;    }
                        case 3: {
                            int nIdx1 = mapNodeIndex[facesHelper[l].Element->GetNode(1)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx5 = mapNodeIndex[facesHelper[l].Element->GetNode(5)];
                            int nIdx8 = mapNodeIndex[facesHelper[l].Element->GetNode(8)];
                            int nIdx9 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx1;
                            indices[index++] = nIdx8;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx1,nIdx5);
                            insEdgeVec(EdgeMap,nIdx1,nIdx8);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,2);
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx9;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx2,nIdx5);
                            insEdgeVec(EdgeMap,nIdx2,nIdx9);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,2);
                            indices[index++] = nIdx3;
                            indices[index++] = nIdx9;
                            indices[index++] = nIdx8;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx3,nIdx9);
                            insEdgeVec(EdgeMap,nIdx3,nIdx8);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,2);
                            indices[index++] = nIdx9;
                            indices[index++] = nIdx5;
                            indices[index++] = nIdx8;
                            indices[index++] = SO_END_FACE_INDEX;
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,2);
                            break;    }
                        case 4: {
                            int nIdx0 = mapNodeIndex[facesHelper[l].Element->GetNode(0)];
                            int nIdx2 = mapNodeIndex[facesHelper[l].Element->GetNode(2)];
                            int nIdx3 = mapNodeIndex[facesHelper[l].Element->GetNode(3)];
                            int nIdx6 = mapNodeIndex[facesHelper[l].Element->GetNode(6)];
                            int nIdx7 = mapNodeIndex[facesHelper[l].Element->GetNode(7)];
                            int nIdx9 = mapNodeIndex[facesHelper[l].Element->GetNode(9)];
                            indices[index++] = nIdx0;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx7;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx0,nIdx6);
                            insEdgeVec(EdgeMap,nIdx0,nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,3);
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx2;
                            indices[index++] = nIdx9;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx2,nIdx6);
                            insEdgeVec(EdgeMap,nIdx2,nIdx9);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,3);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx9;
                            indices[index++] = nIdx3;
                            indices[index++] = SO_END_FACE_INDEX;
                            insEdgeVec(EdgeMap,nIdx3,nIdx9);
                            insEdgeVec(EdgeMap,nIdx3,nIdx7);
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,3);
                            indices[index++] = nIdx7;
                            indices[index++] = nIdx6;
                            indices[index++] = nIdx9;
                            indices[index++] = SO_END_FACE_INDEX;
                            vFaceElementIdx[indexIdx++] = ElemFold(facesHelper[l].ElementNumber,3);
                            break;    }
                        default: assert(0);

                    }
                    break;

                default:assert(0); // not implemented node
            }
        }
    }

    faces->coordIndex.finishEditing();

    Base::Console().Log("    %f: Start build up edge vector\n",Base::TimeInfo::diffTimeF(Start,Base::TimeInfo()));
    // std::map<int,std::set<int> > EdgeMap;
    // count edges
    int EdgeSize = 0;
    for(std::map<int,std::set<int> >::const_iterator it= EdgeMap.begin();it!= EdgeMap.end();++it)
        EdgeSize += it->second.size();

    // set the triangle face indices
    lines->coordIndex.setNum(3*EdgeSize);
    index=0;
    indices = lines->coordIndex.startEditing();

    for(std::map<int,std::set<int> >::const_iterator it= EdgeMap.begin();it!= EdgeMap.end();++it){
        for(std::set<int>::const_iterator it2=it->second.begin();it2!=it->second.end();++it2){
            indices[index++] = it->first;
            indices[index++] = *it2;
            indices[index++] = -1;
        }
    }

    lines->coordIndex.finishEditing();
    Base::Console().Log("    NumEdges:%i\n",EdgeSize);

    Base::Console().Log("    %f: Finish =========================================================\n",Base::TimeInfo::diffTimeF(Start,Base::TimeInfo()));


}
