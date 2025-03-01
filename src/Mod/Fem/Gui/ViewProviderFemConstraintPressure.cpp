/***************************************************************************
 *   Copyright (c) 2015 FreeCAD Developers                                 *
 *   Author: Przemo Firszt <przemo@firszt.eu>                              *
 *   Based on Force constraint by Jan Rheinländer                          *
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
# include <Inventor/nodes/SoSeparator.h>
# include <Inventor/nodes/SoTranslation.h>
# include <Inventor/nodes/SoRotation.h>
# include <Inventor/nodes/SoMultipleCopy.h>
# include <Precision.hxx>
#endif

#include "Mod/Fem/App/FemConstraintPressure.h"
#include "TaskFemConstraintPressure.h"
#include "ViewProviderFemConstraintPressure.h"
#include <Base/Console.h>
#include <Gui/Control.h>

using namespace FemGui;

PROPERTY_SOURCE(FemGui::ViewProviderFemConstraintPressure, FemGui::ViewProviderFemConstraint)

ViewProviderFemConstraintPressure::ViewProviderFemConstraintPressure()
{
    sPixmap = "fem-constraint-pressure";
    ADD_PROPERTY(FaceColor,(0.0f,0.2f,0.8f));
}

ViewProviderFemConstraintPressure::~ViewProviderFemConstraintPressure()
{
}

//FIXME setEdit needs a careful review
bool ViewProviderFemConstraintPressure::setEdit(int ModNum)
{
    if (ModNum == ViewProvider::Default) {
        // When double-clicking on the item for this constraint the
        // object unsets and sets its edit mode without closing
        // the task panel
        Gui::TaskView::TaskDialog *dlg = Gui::Control().activeDialog();
        TaskDlgFemConstraintPressure *constrDlg = qobject_cast<TaskDlgFemConstraintPressure *>(dlg);
        if (constrDlg && constrDlg->getConstraintView() != this)
            constrDlg = 0; // another constraint left open its task panel
        if (dlg && !constrDlg) {
            if (constraintDialog != NULL) {
                // Ignore the request to open another dialog
                return false;
            } else {
                constraintDialog = new TaskFemConstraintPressure(this);
                return true;
            }
        }

        // clear the selection (convenience)
        Gui::Selection().clearSelection();

        // start the edit dialog
        if (constrDlg)
            Gui::Control().showDialog(constrDlg);
        else
            Gui::Control().showDialog(new TaskDlgFemConstraintPressure(this));
        return true;
    }
    else {
        return ViewProviderDocumentObject::setEdit(ModNum);
    }
}

#define ARROWLENGTH 5
#define ARROWHEADRADIUS 3

void ViewProviderFemConstraintPressure::updateData(const App::Property* prop)
{
    // Gets called whenever a property of the attached object changes
    Fem::ConstraintPressure* pcConstraint = static_cast<Fem::ConstraintPressure*>(this->getObject());

    if (pShapeSep->getNumChildren() == 0) {
        // Set up the nodes
        SoMultipleCopy* cp = new SoMultipleCopy();
        cp->ref();
        cp->matrix.setNum(0);
        cp->addChild((SoNode*)createArrow(ARROWLENGTH, ARROWHEADRADIUS));
        pShapeSep->addChild(cp);
    }

    if (strcmp(prop->getName(),"Points") == 0) {
        const std::vector<Base::Vector3d>& points = pcConstraint->Points.getValues();
        const std::vector<Base::Vector3d>& normals = pcConstraint->Normals.getValues();
        if (points.size() != normals.size()) {
            return;
        }
        std::vector<Base::Vector3d>::const_iterator n = normals.begin();

        SoMultipleCopy* cp = static_cast<SoMultipleCopy*>(pShapeSep->getChild(0));
        cp->matrix.setNum(points.size());
        SbMatrix* matrices = cp->matrix.startEditing();
        int idx = 0;

        for (std::vector<Base::Vector3d>::const_iterator p = points.begin(); p != points.end(); p++) {
            SbVec3f base(p->x, p->y, p->z);
            SbVec3f dir(n->x, n->y, n->z);
            double rev;
            if (pcConstraint->Reversed.getValue()) {
                base = base + dir * ARROWLENGTH;
                rev = 1;
            } else {
                rev = -1;
            }
            SbRotation rot(SbVec3f(0, rev, 0), dir);
            SbMatrix m;
            m.setTransform(base, rot, SbVec3f(1,1,1));
            matrices[idx] = m;
            idx++;
            n++;
        }
        cp->matrix.finishEditing();
    }

    ViewProviderFemConstraint::updateData(prop);
}
