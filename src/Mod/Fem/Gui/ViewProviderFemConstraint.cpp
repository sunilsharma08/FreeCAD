/***************************************************************************
 *   Copyright (c) 2013 Jan Rheinländer                                    *
 *                                   <jrheinlaender@users.sourceforge.net> *
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
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMultipleCopy.h>
#include <Inventor/nodes/SoPickStyle.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTranslation.h>
#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QStackedWidget>
#endif

#include "App/Application.h"
#include "Gui/Command.h"
#include "Gui/Control.h"
#include "Gui/Document.h"
#include "Gui/MainWindow.h"
#include "Mod/Fem/App/FemConstraint.h"

#include "TaskFemConstraint.h"
#include "ViewProviderFemConstraint.h"
#include "ViewProviderFemConstraintPy.h"


using namespace FemGui;

PROPERTY_SOURCE(FemGui::ViewProviderFemConstraint, Gui::ViewProviderGeometryObject)


ViewProviderFemConstraint::ViewProviderFemConstraint()
    : rotateSymbol(true)
    , pSymbol(nullptr)
    , pExtraSymbol(nullptr)
    , ivFile(nullptr)
    , wizardWidget(nullptr)
    , wizardSubLayout(nullptr)
    , constraintDialog(nullptr)
{
    pShapeSep = new SoSeparator();
    pShapeSep->ref();
    pMultCopy = new SoMultipleCopy();
    pMultCopy->ref();

    ShapeAppearance.setDiffuseColor(1.0f, 0.0f, 0.2f);
    ShapeAppearance.setSpecularColor(0.0f, 0.0f, 0.0f);

    Gui::ViewProviderSuppressibleExtension::initExtension(this);
}

ViewProviderFemConstraint::~ViewProviderFemConstraint()
{
    pMultCopy->unref();
    pShapeSep->unref();
}

void ViewProviderFemConstraint::attach(App::DocumentObject* pcObject)
{
    ViewProviderGeometryObject::attach(pcObject);

    SoPickStyle* ps = new SoPickStyle();
    ps->style = SoPickStyle::UNPICKABLE;

    SoSeparator* sep = new SoSeparator();
    SoShapeHints* hints = new SoShapeHints();
    hints->shapeType.setValue(SoShapeHints::UNKNOWN_SHAPE_TYPE);
    hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    sep->addChild(ps);
    sep->addChild(hints);
    sep->addChild(pcShapeMaterial);
    sep->addChild(pShapeSep);
    addDisplayMaskMode(sep, "Base");
}

std::string ViewProviderFemConstraint::resourceSymbolDir =
    App::Application::getResourceDir() + "Mod/Fem/Resources/symbols/";

void ViewProviderFemConstraint::loadSymbol(const char* fileName)
{
    ivFile = fileName;
    SoInput in;
    if (!in.openFile(ivFile)) {
        std::stringstream str;
        str << "Error opening symbol file " << fileName;
        throw Base::ImportError(str.str());
    }
    SoSeparator* nodes = SoDB::readAll(&in);
    if (!nodes) {
        std::stringstream str;
        str << "Error reading symbol file " << fileName;
        throw Base::ImportError(str.str());
    }

    nodes->ref();
    pSymbol = dynamic_cast<SoSeparator*>(nodes->getChild(0));
    pShapeSep->addChild(pMultCopy);
    if (pSymbol) {
        pMultCopy->addChild(pSymbol);
    }
    if (nodes->getNumChildren() == 2) {
        pExtraSymbol = dynamic_cast<SoSeparator*>(nodes->getChild(1));
        if (pExtraSymbol) {
            pShapeSep->addChild(pExtraSymbol);
        }
    }
    pMultCopy->matrix.setNum(0);
    nodes->unref();
}

std::vector<std::string> ViewProviderFemConstraint::getDisplayModes() const
{
    // add modes
    std::vector<std::string> StrList;
    StrList.emplace_back("Base");
    return StrList;
}

void ViewProviderFemConstraint::setDisplayMode(const char* ModeName)
{
    if (strcmp(ModeName, "Base") == 0) {
        setDisplayMaskMode("Base");
    }
    ViewProviderDocumentObject::setDisplayMode(ModeName);
}

std::vector<App::DocumentObject*> ViewProviderFemConstraint::claimChildren() const
{
    return {};
}

void ViewProviderFemConstraint::setupContextMenu(QMenu* menu, QObject* receiver, const char* member)
{
    QAction* act;
    act = menu->addAction(QObject::tr("Edit analysis feature"), receiver, member);
    act->setData(QVariant((int)ViewProvider::Default));
    ViewProviderDocumentObject::setupContextMenu(menu,
                                                 receiver,
                                                 member);  // clazy:exclude=skipped-base-method
}

void ViewProviderFemConstraint::onChanged(const App::Property* prop)
{
    ViewProviderGeometryObject::onChanged(prop);
}

void ViewProviderFemConstraint::updateData(const App::Property* prop)
{
    auto pcConstraint = static_cast<const Fem::Constraint*>(this->getObject());

    if (prop == &pcConstraint->Points || prop == &pcConstraint->Normals
        || prop == &pcConstraint->Scale) {
        updateSymbol();
    }
    else {
        ViewProviderGeometryObject::updateData(prop);
    }
}

void ViewProviderFemConstraint::handleChangedPropertyName(Base::XMLReader& reader,
                                                          const char* typeName,
                                                          const char* propName)
{
    if (strcmp(propName, "FaceColor") == 0
        && Base::Type::fromName(typeName) == App::PropertyColor::getClassTypeId()) {
        App::PropertyColor color;
        color.Restore(reader);
        ShapeAppearance.setDiffuseColor(color.getValue());
    }
    else if (strcmp(propName, "ShapeMaterial") == 0
             && Base::Type::fromName(typeName) == App::PropertyMaterial::getClassTypeId()) {
        // nothing
    }
    else {
        ViewProviderGeometryObject::handleChangedPropertyName(reader, typeName, propName);
    }
}

void ViewProviderFemConstraint::updateSymbol()
{
    auto obj = static_cast<const Fem::Constraint*>(this->getObject());
    const std::vector<Base::Vector3d>& points = obj->Points.getValue();
    const std::vector<Base::Vector3d>& normals = obj->Normals.getValue();
    if (points.size() != normals.size()) {
        return;
    }

    pMultCopy->matrix.setNum(points.size());
    SbMatrix* mat = pMultCopy->matrix.startEditing();

    for (size_t i = 0; i < points.size(); ++i) {
        transformSymbol(points[i], normals[i], mat[i]);
    }

    pMultCopy->matrix.finishEditing();
}

void ViewProviderFemConstraint::transformSymbol(const Base::Vector3d& point,
                                                const Base::Vector3d& normal,
                                                SbMatrix& mat) const
{
    auto obj = static_cast<const Fem::Constraint*>(this->getObject());
    SbVec3f axisY(0, 1, 0);
    float s = obj->getScaleFactor();
    SbVec3f scale(s, s, s);
    SbVec3f norm = rotateSymbol ? SbVec3f(normal.x, normal.y, normal.z) : axisY;
    SbRotation rot(axisY, norm);
    SbVec3f tra(static_cast<float>(point.x),
                static_cast<float>(point.y),
                static_cast<float>(point.z));
    mat.setTransform(tra, rot, scale);
}


// OvG: Visibility automation show parts and hide meshes on activation of a constraint
std::string ViewProviderFemConstraint::gethideMeshShowPartStr(const std::string showConstr)
{
    return "for amesh in App.activeDocument().Objects:\n\
    if \""
        + showConstr + "\" == amesh.Name:\n\
        amesh.ViewObject.Visibility = True\n\
    elif \"Mesh\" in amesh.TypeId:\n\
        amesh.ViewObject.Visibility = False\n";
}

std::string ViewProviderFemConstraint::gethideMeshShowPartStr()
{
    return ViewProviderFemConstraint::gethideMeshShowPartStr("");
}

bool ViewProviderFemConstraint::setEdit(int ModNum)
{
    Gui::Command::doCommand(Gui::Command::Doc,
                            "%s",
                            ViewProviderFemConstraint::gethideMeshShowPartStr().c_str());
    return Gui::ViewProviderGeometryObject::setEdit(ModNum);
}

void ViewProviderFemConstraint::unsetEdit(int ModNum)
{
    // clear the selection (convenience)
    Gui::Selection().clearSelection();

    if (wizardWidget && wizardSubLayout && constraintDialog) {
        wizardWidget = nullptr;
        wizardSubLayout = nullptr;
        delete constraintDialog;
        constraintDialog = nullptr;

        // Notify the Shaft Wizard that we have finished editing
        // See WizardShaft.py on why we do it this way
        Gui::Command::runCommand(Gui::Command::Doc,
                                 "Gui.runCommand('PartDesign_WizardShaftCallBack')");
    }
    else {
        if (ModNum == ViewProvider::Default) {
            // when pressing ESC make sure to close the dialog
            Gui::Control().closeDialog();
        }
        else {
            ViewProviderGeometryObject::unsetEdit(ModNum);
        }
    }
}

PyObject* ViewProviderFemConstraint::getPyObject()
{
    if (!pyViewObject) {
        pyViewObject = new ViewProviderFemConstraintPy(this);
    }
    pyViewObject->IncRef();
    return pyViewObject;
}
/*
// Create a local coordinate system with the z-axis given in dir
void getLocalCoordinateSystem(const SbVec3f& z, SbVec3f& y, SbVec3f& x)
{
    // Find the y axis in an arbitrary direction, normal to z
    // Conditions:
    // y1 * z1 + y2 * z2 + y3 * z3 = |y| |z| cos(90°) = 0
    // |y| = sqrt(y1^2 + y2^2 + y3^2) = 1
    float z1, z2, z3;
    z.getValue(z1, z2, z3);
    float y1, y2, y3;
    if (fabs(z1) > Precision::Confusion()) {
        // Choose: y3 = 0
        // Solution:
        // y1 * z1 + y2 * z2 = 0
        // y1 = - z2/z1 y2
        // sqrt(z2^2/z1^2 y2^2 + y2^2) = 1
        // y2^2 ( 1 + z2^2/z1^2)) = +-1 -> choose +1 otherwise no solution
        // y2 = +- sqrt(1 / (1 + z2^2/z1^2))
        y3 = 0;
        y2 = sqrt(1 / (1 + z2*z2 / (z1*z1)));
        y1 = -z2/z1 * y2;
        // Note: result might be (0, 1, 0)
    } else if (fabs(z2) > Precision::Confusion()) {
        // Given: z1 = 0
        // Choose: y1 = 0
        // Solution:
        // y2 * z2 + y3 * z3 = 0
        // y2 = - z3/z2 y3
        // sqrt(z3^2/z2^2 y3^3 + y3^2) = 1
        // y3^2 (1 + z3^2/z2^2)) = +1
        // y3 = +- sqrt(1 / (1 + z3^2/z2^2))
        y1 = 0;
        y3 = sqrt(1 / (1 + z3*z3 / (z2*z2)));
        y2 = -z3/z2 * y3;
        // Note: result might be (0, 0, 1)
    } else if  (fabs(z3) > Precision::Confusion()) {
        // Given: z1 = z2 = 0
        // Choose the remaining possible axis
        y1 = 1;
        y2 = 0;
        y3 = 0;
    }

    y = SbVec3f(y1, y2, y3);
    x = y.cross(z);
}
*/
#define PLACEMENT_CHILDREN 2

void ViewProviderFemConstraint::createPlacement(SoSeparator* sep,
                                                const SbVec3f& base,
                                                const SbRotation& r)
{
    SoTranslation* trans = new SoTranslation();
    trans->translation.setValue(base);
    sep->addChild(trans);
    SoRotation* rot = new SoRotation();
    rot->rotation.setValue(r);
    sep->addChild(rot);
}

void ViewProviderFemConstraint::updatePlacement(const SoSeparator* sep,
                                                const int idx,
                                                const SbVec3f& base,
                                                const SbRotation& r)
{
    SoTranslation* trans = static_cast<SoTranslation*>(sep->getChild(idx));
    trans->translation.setValue(base);
    SoRotation* rot = static_cast<SoRotation*>(sep->getChild(idx + 1));
    rot->rotation.setValue(r);
}

#define CONE_CHILDREN 2

void ViewProviderFemConstraint::createCone(SoSeparator* sep,
                                           const double height,
                                           const double radius)
{
    // Adjust cone so that the tip is on base
    SoTranslation* trans = new SoTranslation();
    trans->translation.setValue(SbVec3f(0, -height / 2, 0));
    sep->addChild(trans);
    SoCone* cone = new SoCone();
    cone->height.setValue(height);
    cone->bottomRadius.setValue(radius);
    sep->addChild(cone);
}

SoSeparator* ViewProviderFemConstraint::createCone(const double height, const double radius)
{
    // Create a new cone node
    SoSeparator* sep = new SoSeparator();
    createCone(sep, height, radius);
    return sep;
}

void ViewProviderFemConstraint::updateCone(const SoNode* node,
                                           const int idx,
                                           const double height,
                                           const double radius)
{
    const SoSeparator* sep = static_cast<const SoSeparator*>(node);
    SoTranslation* trans = static_cast<SoTranslation*>(sep->getChild(idx));
    trans->translation.setValue(SbVec3f(0, -height / 2, 0));
    SoCone* cone = static_cast<SoCone*>(sep->getChild(idx + 1));
    cone->height.setValue(height);
    cone->bottomRadius.setValue(radius);
}

#define CYLINDER_CHILDREN 1

void ViewProviderFemConstraint::createCylinder(SoSeparator* sep,
                                               const double height,
                                               const double radius)
{
    SoCylinder* cyl = new SoCylinder();
    cyl->height.setValue(height);
    cyl->radius.setValue(radius);
    sep->addChild(cyl);
}

SoSeparator* ViewProviderFemConstraint::createCylinder(const double height, const double radius)
{
    // Create a new cylinder node
    SoSeparator* sep = new SoSeparator();
    createCylinder(sep, height, radius);
    return sep;
}

void ViewProviderFemConstraint::updateCylinder(const SoNode* node,
                                               const int idx,
                                               const double height,
                                               const double radius)
{
    const SoSeparator* sep = static_cast<const SoSeparator*>(node);
    SoCylinder* cyl = static_cast<SoCylinder*>(sep->getChild(idx));
    cyl->height.setValue(height);
    cyl->radius.setValue(radius);
}

#define CUBE_CHILDREN 1

void ViewProviderFemConstraint::createCube(SoSeparator* sep,
                                           const double width,
                                           const double length,
                                           const double height)
{
    SoCube* cube = new SoCube();
    cube->width.setValue(width);
    cube->depth.setValue(length);
    cube->height.setValue(height);
    sep->addChild(cube);
}

SoSeparator*
ViewProviderFemConstraint::createCube(const double width, const double length, const double height)
{
    SoSeparator* sep = new SoSeparator();
    createCube(sep, width, length, height);
    return sep;
}

void ViewProviderFemConstraint::updateCube(const SoNode* node,
                                           const int idx,
                                           const double width,
                                           const double length,
                                           const double height)
{
    const SoSeparator* sep = static_cast<const SoSeparator*>(node);
    SoCube* cube = static_cast<SoCube*>(sep->getChild(idx));
    cube->width.setValue(width);
    cube->depth.setValue(length);
    cube->height.setValue(height);
}

#define ARROW_CHILDREN (CONE_CHILDREN + PLACEMENT_CHILDREN + CYLINDER_CHILDREN)

void ViewProviderFemConstraint::createArrow(SoSeparator* sep,
                                            const double length,
                                            const double radius)
{
    createCone(sep, radius, radius / 2);
    createPlacement(sep, SbVec3f(0, -radius / 2 - (length - radius) / 2, 0), SbRotation());
    createCylinder(sep, length - radius, radius / 5);
}

SoSeparator* ViewProviderFemConstraint::createArrow(const double length, const double radius)
{
    SoSeparator* sep = new SoSeparator();
    createArrow(sep, length, radius);
    return sep;
}

void ViewProviderFemConstraint::updateArrow(const SoNode* node,
                                            const int idx,
                                            const double length,
                                            const double radius)
{
    const SoSeparator* sep = static_cast<const SoSeparator*>(node);
    updateCone(sep, idx, radius, radius / 2);
    updatePlacement(sep,
                    idx + CONE_CHILDREN,
                    SbVec3f(0, -radius / 2 - (length - radius) / 2, 0),
                    SbRotation());
    updateCylinder(sep, idx + CONE_CHILDREN + PLACEMENT_CHILDREN, length - radius, radius / 5);
}

#define SPRING_CHILDREN (CUBE_CHILDREN + PLACEMENT_CHILDREN + CYLINDER_CHILDREN)

void ViewProviderFemConstraint::createSpring(SoSeparator* sep,
                                             const double length,
                                             const double width)
{
    createCube(sep, width, width, length / 2);
    createPlacement(sep, SbVec3f(0, -length / 2, 0), SbRotation());
    createCylinder(sep, length / 2, width / 4);
}

SoSeparator* ViewProviderFemConstraint::createSpring(const double length, const double width)
{
    SoSeparator* sep = new SoSeparator();
    createSpring(sep, length, width);
    return sep;
}

void ViewProviderFemConstraint::updateSpring(const SoNode* node,
                                             const int idx,
                                             const double length,
                                             const double width)
{
    const SoSeparator* sep = static_cast<const SoSeparator*>(node);
    updateCube(sep, idx, width, width, length / 2);
    updatePlacement(sep, idx + CUBE_CHILDREN, SbVec3f(0, -length / 2, 0), SbRotation());
    updateCylinder(sep, idx + CUBE_CHILDREN + PLACEMENT_CHILDREN, length / 2, width / 4);
}

#define FIXED_CHILDREN (CONE_CHILDREN + PLACEMENT_CHILDREN + CUBE_CHILDREN)

void ViewProviderFemConstraint::createFixed(SoSeparator* sep,
                                            const double height,
                                            const double width,
                                            const bool gap)
{
    createCone(sep, height - width / 4, height - width / 4);
    createPlacement(
        sep,
        SbVec3f(0, -(height - width / 4) / 2 - width / 8 - (gap ? 1.0 : 0.1) * width / 8, 0),
        SbRotation());
    createCube(sep, width, width, width / 4);
}

SoSeparator*
ViewProviderFemConstraint::createFixed(const double height, const double width, const bool gap)
{
    SoSeparator* sep = new SoSeparator();
    createFixed(sep, height, width, gap);
    return sep;
}

void ViewProviderFemConstraint::updateFixed(const SoNode* node,
                                            const int idx,
                                            const double height,
                                            const double width,
                                            const bool gap)
{
    const SoSeparator* sep = static_cast<const SoSeparator*>(node);
    updateCone(sep, idx, height - width / 4, height - width / 4);
    updatePlacement(
        sep,
        idx + CONE_CHILDREN,
        SbVec3f(0, -(height - width / 4) / 2 - width / 8 - (gap ? 1.0 : 0.0) * width / 8, 0),
        SbRotation());
    updateCube(sep, idx + CONE_CHILDREN + PLACEMENT_CHILDREN, width, width, width / 4);
}

void ViewProviderFemConstraint::createDisplacement(SoSeparator* sep,
                                                   const double height,
                                                   const double width,
                                                   const bool gap)
{
    createCone(sep, height, width);
    createPlacement(sep,
                    SbVec3f(0, -(height) / 2 - width / 8 - (gap ? 1.0 : 0.1) * width / 8, 0),
                    SbRotation());
}

SoSeparator* ViewProviderFemConstraint::createDisplacement(const double height,
                                                           const double width,
                                                           const bool gap)
{
    SoSeparator* sep = new SoSeparator();
    createDisplacement(sep, height, width, gap);
    return sep;
}

void ViewProviderFemConstraint::updateDisplacement(const SoNode* node,
                                                   const int idx,
                                                   const double height,
                                                   const double width,
                                                   const bool gap)
{
    const SoSeparator* sep = static_cast<const SoSeparator*>(node);
    updateCone(sep, idx, height, width);
    updatePlacement(sep,
                    idx + CONE_CHILDREN,
                    SbVec3f(0, -(height) / 2 - width / 8 - (gap ? 1.0 : 0.0) * width / 8, 0),
                    SbRotation());
}

void ViewProviderFemConstraint::createRotation(SoSeparator* sep,
                                               const double height,
                                               const double width,
                                               const bool gap)
{
    createCylinder(sep, width / 2, height / 2);
    createPlacement(sep,
                    SbVec3f(0, -(height) * 2 - width / 8 - (gap ? 1.0 : 0.1) * width / 8, 0),
                    SbRotation());
}

SoSeparator*
ViewProviderFemConstraint::createRotation(const double height, const double width, const bool gap)
{
    SoSeparator* sep = new SoSeparator();
    createRotation(sep, height, width, gap);
    return sep;
}

void ViewProviderFemConstraint::updateRotation(const SoNode* node,
                                               const int idx,
                                               const double height,
                                               const double width,
                                               const bool gap)
{
    const SoSeparator* sep = static_cast<const SoSeparator*>(node);
    updateCylinder(sep, idx, height / 2, width / 2);
    updatePlacement(sep,
                    idx + CYLINDER_CHILDREN,
                    SbVec3f(0, -(height) * 2 - width / 8 - (gap ? 1.0 : 0.0) * width / 8, 0),
                    SbRotation());
}

QObject* ViewProviderFemConstraint::findChildByName(const QObject* parent, const QString& name)
{
    for (auto o : parent->children()) {
        if (o->objectName() == name) {
            return o;
        }
        if (!o->children().empty()) {
            QObject* result = findChildByName(o, name);
            if (result) {
                return result;
            }
        }
    }

    return nullptr;
}

void ViewProviderFemConstraint::checkForWizard()
{
    wizardWidget = nullptr;
    wizardSubLayout = nullptr;
    Gui::MainWindow* mw = Gui::getMainWindow();
    if (!mw) {
        return;
    }
    QDockWidget* dw = mw->findChild<QDockWidget*>(QString::fromLatin1("Combo View"));
    if (!dw) {
        return;
    }
    QWidget* cw = dw->findChild<QWidget*>(QString::fromLatin1("Combo View"));
    if (!cw) {
        return;
    }
    QTabWidget* tw = cw->findChild<QTabWidget*>(QString::fromLatin1("combiTab"));
    if (!tw) {
        return;
    }
    QStackedWidget* sw =
        tw->findChild<QStackedWidget*>(QString::fromLatin1("qt_tabwidget_stackedwidget"));
    if (!sw) {
        return;
    }
    QScrollArea* sa = sw->findChild<QScrollArea*>();
    if (!sa) {
        return;
    }
    QWidget* wd =
        sa->widget();  // This is the reason why we cannot use findChildByName() right away!!!
    if (!wd) {
        return;
    }
    QObject* wiz = findChildByName(wd, QString::fromLatin1("ShaftWizard"));
    if (wiz) {
        wizardWidget = static_cast<QVBoxLayout*>(wiz);
        wizardSubLayout = wiz->findChild<QVBoxLayout*>(QString::fromLatin1("ShaftWizardLayout"));
    }
}


// Python feature -----------------------------------------------------------------------

namespace Gui
{
/// @cond DOXERR
PROPERTY_SOURCE_TEMPLATE(FemGui::ViewProviderFemConstraintPython, FemGui::ViewProviderFemConstraint)
/// @endcond

// explicit template instantiation
template class FemGuiExport ViewProviderPythonFeatureT<ViewProviderFemConstraint>;
}  // namespace Gui
