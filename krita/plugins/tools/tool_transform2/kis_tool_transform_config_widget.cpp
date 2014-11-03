/*
 *  Copyright (c) 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_tool_transform_config_widget.h"

#include <KoIcon.h>
#include "rotation_icons.h"
#include "kis_canvas2.h"
#include <QSignalMapper>
#include "kis_liquify_properties.h"

#include "KoMainWindow.h"
#include "kis_view2.h"


template<typename T> inline T sign(T x) {
    return x > 0 ? 1 : x == (T)0 ? 0 : -1;
}

const int KisToolTransformConfigWidget::DEFAULT_POINTS_PER_LINE = 3;


KisToolTransformConfigWidget::KisToolTransformConfigWidget(TransformTransactionProperties *transaction, KisCanvas2 *canvas, bool workRecursively, QWidget *parent)
    : QWidget(parent),
      m_transaction(transaction),
      m_notificationsBlocked(0),
      m_uiSlotsBlocked(0),
      m_configChanged(false)
{
    setupUi(this);
    showDecorationsBox->setIcon(koIcon("krita_tool_transform"));
    chkWorkRecursively->setIcon(koIcon("krita_tool_transform_recursive.png"));
    label_shearX->setPixmap(koIcon("shear_horizontal").pixmap(16, 16));
    label_shearY->setPixmap(koIcon("shear_vertical").pixmap(16, 16));

    label_width->setPixmap(koIcon("width_icon").pixmap(16, 16));
    label_height->setPixmap(koIcon("height_icon").pixmap(16, 16));

    label_offsetX->setPixmap(koIcon("offset_horizontal").pixmap(16, 16));
    label_offsetY->setPixmap(koIcon("offset_vertical").pixmap(16, 16));

    // Init labels
    QPixmap rotateX_Pixmap, rotateY_Pixmap, rotateZ_Pixmap;
    rotateX_Pixmap.loadFromData(rotateX_PNG, rotateX_PNG_len, "png");
    rotateY_Pixmap.loadFromData(rotateY_PNG, rotateY_PNG_len, "png");
    rotateZ_Pixmap.loadFromData(rotateZ_PNG, rotateZ_PNG_len, "png");
    label_rotateX->setPixmap(rotateX_Pixmap);
    label_rotateY->setPixmap(rotateY_Pixmap);
    label_rotateZ->setPixmap(rotateZ_Pixmap);

    chkWorkRecursively->setChecked(workRecursively);
    connect(chkWorkRecursively, SIGNAL(toggled(bool)), this, SIGNAL(sigRestartTransform()));

    // Init Filter  combo
    cmbFilter->setIDList(KisFilterStrategyRegistry::instance()->listKeys());
    cmbFilter->setCurrent("Bicubic");
    connect(cmbFilter, SIGNAL(activated(const KoID &)),
            this, SLOT(slotFilterChanged(const KoID &)));

    // Init Warp Type combo
    cmbWarpType->insertItem(KisWarpTransformWorker::AFFINE_TRANSFORM,i18n("Default (Affine)"));
    cmbWarpType->insertItem(KisWarpTransformWorker::RIGID_TRANSFORM,i18n("Strong (Rigid)"));
    cmbWarpType->insertItem(KisWarpTransformWorker::SIMILITUDE_TRANSFORM,i18n("Strongest (Similitude)"));
    cmbWarpType->setCurrentIndex(KisWarpTransformWorker::AFFINE_TRANSFORM);
    connect(cmbWarpType, SIGNAL(currentIndexChanged(int)), this, SLOT(slotWarpTypeChanged(int)));

    // Init Rotation Center buttons
    m_handleDir[0] = QPointF(1, 0);
    m_handleDir[1] = QPointF(1, -1);
    m_handleDir[2] = QPointF(0, -1);
    m_handleDir[3] = QPointF(-1, -1);
    m_handleDir[4] = QPointF(-1, 0);
    m_handleDir[5] = QPointF(-1, 1);
    m_handleDir[6] = QPointF(0, 1);
    m_handleDir[7] = QPointF(1, 1);
    m_handleDir[8] = QPointF(0, 0); // also add the center

    m_rotationCenterButtons = new QButtonGroup(0);
    // we set the ids to match m_handleDir
    m_rotationCenterButtons->addButton(middleRightButton, 0);
    m_rotationCenterButtons->addButton(topRightButton, 1);
    m_rotationCenterButtons->addButton(middleTopButton, 2);
    m_rotationCenterButtons->addButton(topLeftButton, 3);
    m_rotationCenterButtons->addButton(middleLeftButton, 4);
    m_rotationCenterButtons->addButton(bottomLeftButton, 5);
    m_rotationCenterButtons->addButton(middleBottomButton, 6);
    m_rotationCenterButtons->addButton(bottomRightButton, 7);
    m_rotationCenterButtons->addButton(centerButton, 8);

    QToolButton *nothingSelected = new QToolButton(0);
    nothingSelected->setCheckable(true);
    nothingSelected->setAutoExclusive(true);
    nothingSelected->hide(); // a convenient button for when no button is checked in the group
    m_rotationCenterButtons->addButton(nothingSelected, 9);

    connect(m_rotationCenterButtons, SIGNAL(buttonPressed(int)), this, SLOT(slotRotationCenterChanged(int)));

    // Init Free Transform Values
    connect(scaleXBox, SIGNAL(valueChanged(double)), this, SLOT(slotSetScaleX(double)));
    connect(scaleYBox, SIGNAL(valueChanged(double)), this, SLOT(slotSetScaleY(double)));
    connect(shearXBox, SIGNAL(valueChanged(double)), this, SLOT(slotSetShearX(double)));
    connect(shearYBox, SIGNAL(valueChanged(double)), this, SLOT(slotSetShearY(double)));
    connect(translateXBox, SIGNAL(valueChanged(double)), this, SLOT(slotSetTranslateX(double)));
    connect(translateYBox, SIGNAL(valueChanged(double)), this, SLOT(slotSetTranslateY(double)));
    connect(aXBox, SIGNAL(valueChanged(double)), this, SLOT(slotSetAX(double)));
    connect(aYBox, SIGNAL(valueChanged(double)), this, SLOT(slotSetAY(double)));
    connect(aZBox, SIGNAL(valueChanged(double)), this, SLOT(slotSetAZ(double)));
    connect(aspectButton, SIGNAL(keepAspectRatioChanged(bool)), this, SLOT(slotSetKeepAspectRatio(bool)));

    // Init Warp Transform Values  
    alphaBox->setMaximum(5.0);
    alphaBox->setSingleStep(0.1);

    connect(alphaBox, SIGNAL(valueChanged(qreal)), this, SLOT(slotSetWarpAlpha(qreal)));
    connect(densityBox, SIGNAL(valueChanged(int)), this, SLOT(slotSetWarpDensity(int)));

    connect(defaultRadioButton, SIGNAL(clicked(bool)), this, SLOT(slotWarpDefaultPointsButtonClicked(bool)));
    connect(customRadioButton, SIGNAL(clicked(bool)), this, SLOT(slotWarpCustomPointsButtonClicked(bool)));
    connect(lockUnlockPointsButton, SIGNAL(clicked()), this, SLOT(slotWarpLockPointsButtonClicked()));
    connect(resetPointsButton, SIGNAL(clicked()), this, SLOT(slotWarpResetPointsButtonClicked()));

    // Init Cage Transform Values
    cageTransformButtonGroup->setId(cageAddEditRadio, 0); // we need to set manually since Qt Designer generates negative by default
    cageTransformButtonGroup->setId(cageDeformRadio, 1);
    connect(cageTransformButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(slotCageOptionsChanged(int)));

    // Init Liquify Transform Values
    liquifySizeSlider->setRange(KisLiquifyProperties::minSize(),
                                KisLiquifyProperties::maxSize(), 2);
    liquifySizeSlider->setExponentRatio(4);
    liquifySizeSlider->setValue(50.0);
    connect(liquifySizeSlider, SIGNAL(valueChanged(qreal)), this, SLOT(liquifySizeChanged(qreal)));
    liquifySizeSlider->setToolTip(i18nc("@info:tooltip", "Size of the deformation brush"));

    liquifyAmountSlider->setRange(0.0, 1.0, 2);
    liquifyAmountSlider->setValue(1.0);
    connect(liquifyAmountSlider, SIGNAL(valueChanged(qreal)), this, SLOT(liquifyAmountChanged(qreal)));
    liquifyAmountSlider->setToolTip(i18nc("@info:tooltip", "Amount of the deformation you get"));

    liquifyFlowSlider->setRange(0.0, 1.0, 2);
    liquifyFlowSlider->setValue(1.0);
    connect(liquifyFlowSlider, SIGNAL(valueChanged(qreal)), this, SLOT(liquifyFlowChanged(qreal)));
    liquifyFlowSlider->setToolTip(i18nc("@info:tooltip", "When in non-buildup mode, shows how fast the deformation limit is reached."));


    buidupModeComboBox->setCurrentIndex(0); // set to build-up mode by default
    connect(buidupModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(liquifyBuildUpChanged(int)));
    buidupModeComboBox->setToolTip(i18nc("@info:tooltip", "Switch between Build Up and Wash mode of painting. Build Up mode adds deformations one on top of the other without any limits. Wash mode gradually deforms the piece to the selected deformation level."));


    liquifySpacingSlider->setRange(0.0, 3.0, 2);
    liquifySizeSlider->setExponentRatio(3);
    liquifySpacingSlider->setSingleStep(0.01);
    liquifySpacingSlider->setValue(0.2);
    connect(liquifySpacingSlider, SIGNAL(valueChanged(qreal)), this, SLOT(liquifySpacingChanged(qreal)));
    liquifySpacingSlider->setToolTip(i18nc("@info:tooltip", "Space between two sequential applications of the deformation"));

    liquifySizePressureBox->setChecked(true);
    connect(liquifySizePressureBox, SIGNAL(toggled(bool)), this, SLOT(liquifySizePressureChanged(bool)));
    liquifySizePressureBox->setToolTip(i18nc("@info:tooltip", "Scale <interface>Size</interface> value according to current stylus pressure"));

    liquifyAmountPressureBox->setChecked(true);
    connect(liquifyAmountPressureBox, SIGNAL(toggled(bool)), this, SLOT(liquifyAmountPressureChanged(bool)));
    liquifyAmountPressureBox->setToolTip(i18nc("@info:tooltip", "Scale <interface>Amount</interface> value according to current stylus pressure"));

    liquifyReverseDirectionChk->setChecked(false);
    connect(liquifyReverseDirectionChk, SIGNAL(toggled(bool)), this, SLOT(liquifyReverseDirectionChanged(bool)));
    liquifyReverseDirectionChk->setToolTip(i18nc("@info:tooltip", "Reverse direction of the current deformation tool"));

    QSignalMapper *liquifyModeMapper = new QSignalMapper(this);
    connect(liquifyMove, SIGNAL(toggled(bool)), liquifyModeMapper, SLOT(map()));
    connect(liquifyScale, SIGNAL(toggled(bool)), liquifyModeMapper, SLOT(map()));
    connect(liquifyRotate, SIGNAL(toggled(bool)), liquifyModeMapper, SLOT(map()));
    connect(liquifyOffset, SIGNAL(toggled(bool)), liquifyModeMapper, SLOT(map()));
    connect(liquifyUndo, SIGNAL(toggled(bool)), liquifyModeMapper, SLOT(map()));
    liquifyModeMapper->setMapping(liquifyMove, (int)KisLiquifyProperties::MOVE);
    liquifyModeMapper->setMapping(liquifyScale, (int)KisLiquifyProperties::SCALE);
    liquifyModeMapper->setMapping(liquifyRotate, (int)KisLiquifyProperties::ROTATE);
    liquifyModeMapper->setMapping(liquifyOffset, (int)KisLiquifyProperties::OFFSET);
    liquifyModeMapper->setMapping(liquifyUndo, (int)KisLiquifyProperties::UNDO);
    connect(liquifyModeMapper, SIGNAL(mapped(int)), SLOT(slotLiquifyModeChanged(int)));

    liquifyMove->setToolTip(i18nc("@info:tooltip", "Move: drag the image along the brush stroke"));
    liquifyScale->setToolTip(i18nc("@info:tooltip", "Scale: grow/shrink image under cursor"));
    liquifyRotate->setToolTip(i18nc("@info:tooltip", "Rotate: twirl image under cursor"));
    liquifyOffset->setToolTip(i18nc("@info:tooltip", "Offset: shift the image to the right of the stroke direction"));
    liquifyUndo->setToolTip(i18nc("@info:tooltip", "Undo: erase actions of other tools"));

    // Connect all edit boxes to the Editing Finished signal
    connect(scaleXBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));
    connect(scaleYBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));
    connect(shearXBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));
    connect(shearYBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));
    connect(translateXBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));
    connect(translateYBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));
    connect(aXBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));
    connect(aYBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));
    connect(aZBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));   
    connect(densityBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));

    connect(alphaBox, SIGNAL(editingFinished()), this, SLOT(notifyEditingFinished()));

    // Connect other widget (not having editingFinished signal) to
    // the same slot. From Qt 4.6 onwards the sequence of the signal
    // delivery is definite.
    connect(cmbFilter, SIGNAL(activated(const KoID &)), this, SLOT(notifyEditingFinished()));
    connect(cmbWarpType, SIGNAL(currentIndexChanged(int)), this, SLOT(notifyEditingFinished()));
    connect(m_rotationCenterButtons, SIGNAL(buttonPressed(int)), this, SLOT(notifyEditingFinished()));
    connect(aspectButton, SIGNAL(keepAspectRatioChanged(bool)), this, SLOT(notifyEditingFinished()));
    connect(defaultRadioButton, SIGNAL(clicked(bool)), this, SLOT(notifyEditingFinished()));
    connect(customRadioButton, SIGNAL(clicked(bool)), this, SLOT(notifyEditingFinished()));
    connect(lockUnlockPointsButton, SIGNAL(clicked()), this, SLOT(notifyEditingFinished()));
    connect(resetPointsButton, SIGNAL(clicked()), this, SLOT(notifyEditingFinished()));

    connect(liquifySizeSlider, SIGNAL(valueChanged(qreal)), this, SLOT(notifyEditingFinished()));
    connect(liquifyAmountSlider, SIGNAL(valueChanged(qreal)), this, SLOT(notifyEditingFinished()));
    connect(liquifySpacingSlider, SIGNAL(valueChanged(qreal)), this, SLOT(notifyEditingFinished()));
    connect(liquifySizePressureBox, SIGNAL(toggled(bool)), this, SLOT(notifyEditingFinished()));
    connect(liquifyAmountPressureBox, SIGNAL(toggled(bool)), this, SLOT(notifyEditingFinished()));
    connect(liquifyReverseDirectionChk, SIGNAL(toggled(bool)), this, SLOT(notifyEditingFinished()));
    connect(liquifyModeMapper, SIGNAL(mapped(int)), SLOT(notifyEditingFinished()));

    // Connect Apply/Reset buttons
    connect(buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(slotButtonBoxClicked(QAbstractButton *)));

    // Mode switch buttons
    connect(freeTransformButton, SIGNAL(clicked(bool)), this, SLOT(slotSetFreeTransformModeButtonClicked(bool)));
    connect(warpButton, SIGNAL(clicked(bool)), this, SLOT(slotSetWarpModeButtonClicked(bool)));
    connect(cageButton, SIGNAL(clicked(bool)), this, SLOT(slotSetCageModeButtonClicked(bool)));
    connect(perspectiveTransformButton, SIGNAL(clicked(bool)), this, SLOT(slotSetPerspectiveModeButtonClicked(bool)));
    connect(liquifyButton, SIGNAL(clicked(bool)), this, SLOT(slotSetLiquifyModeButtonClicked(bool)));

    // Connect Decorations switcher
    connect(showDecorationsBox, SIGNAL(toggled(bool)), canvas, SLOT(updateCanvas()));

    tooBigLabelWidget->hide();

    connect(canvas->view()->mainWindow(), SIGNAL(themeChanged()), SLOT(slotUpdateIcons()));
    slotUpdateIcons();
}

void KisToolTransformConfigWidget::slotUpdateIcons()
{
    freeTransformButton->setIcon(themedIcon("transform_icons_main"));
    warpButton->setIcon(themedIcon("transform_icons_warp"));
    cageButton->setIcon(themedIcon("transform_icons_cage"));
    perspectiveTransformButton->setIcon(themedIcon("transform_icons_perspective"));
    liquifyButton->setIcon(themedIcon("transform_icons_liquify_main"));

    liquifyMove->setIcon(themedIcon("transform_icons_liquify_move"));
    liquifyScale->setIcon(themedIcon("transform_icons_liquify_resize"));
    liquifyRotate->setIcon(themedIcon("transform_icons_liquify_rotate"));
    liquifyOffset->setIcon(themedIcon("transform_icons_liquify_offset"));
    liquifyUndo->setIcon(themedIcon("transform_icons_liquify_erase"));

    // pressure icons
    liquifySizePressureBox->setIcon(themedIcon("transform_icons_penPressure"));
    liquifyAmountPressureBox->setIcon(themedIcon("transform_icons_penPressure"));
}

double KisToolTransformConfigWidget::radianToDegree(double rad)
{
    double piX2 = 2 * M_PI;

    if (rad < 0 || rad >= piX2) {
        rad = fmod(rad, piX2);
        if (rad < 0) {
            rad += piX2;
        }
    }

    return (rad * 360. / piX2);
}

double KisToolTransformConfigWidget::degreeToRadian(double degree)
{
    if (degree < 0. || degree >= 360.) {
        degree = fmod(degree, 360.);
        if (degree < 0)
            degree += 360.;
    }

    return (degree * M_PI / 180.);
}

void KisToolTransformConfigWidget::updateLiquifyControls()
{
    blockUiSlots();

    ToolTransformArgs *config = m_transaction->currentConfig();
    KisLiquifyProperties *props =
        config->liquifyProperties();

    const bool useWashMode = props->useWashMode();

    liquifySizeSlider->setValue(props->size());
    liquifyAmountSlider->setValue(props->amount());
    liquifyFlowSlider->setValue(props->flow());
    buidupModeComboBox->setCurrentIndex(!useWashMode);

    liquifySpacingSlider->setValue(props->spacing());
    liquifySizePressureBox->setChecked(props->sizeHasPressure());
    liquifyAmountPressureBox->setChecked(props->amountHasPressure());
    liquifyReverseDirectionChk->setChecked(props->reverseDirection());


    KisLiquifyProperties::LiquifyMode mode =
        static_cast<KisLiquifyProperties::LiquifyMode>(props->mode());

    bool canInverseDirection =
        mode != KisLiquifyProperties::UNDO;

    bool canUseWashMode = mode != KisLiquifyProperties::UNDO;

    liquifyReverseDirectionChk->setEnabled(canInverseDirection);
    liquifyFlowSlider->setEnabled(canUseWashMode && useWashMode);
    buidupModeComboBox->setEnabled(canUseWashMode);

    const qreal maxAmount = canUseWashMode ? 5.0 : 1.0;
    liquifyAmountSlider->setRange(0.0, maxAmount, 2);

    unblockUiSlots();
}

void KisToolTransformConfigWidget::slotLiquifyModeChanged(int value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();

    KisLiquifyProperties *props =
        config->liquifyProperties();

    KisLiquifyProperties::LiquifyMode mode =
        static_cast<KisLiquifyProperties::LiquifyMode>(value);

    if (mode == props->mode()) return;

    props->setMode(mode);
    props->loadMode();

    updateLiquifyControls();

    notifyConfigChanged();
}

void KisToolTransformConfigWidget::liquifySizeChanged(qreal value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    KisLiquifyProperties *props =
        config->liquifyProperties();

    props->setSize(value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::liquifyAmountChanged(qreal value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    KisLiquifyProperties *props =
        config->liquifyProperties();

    props->setAmount(value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::liquifyFlowChanged(qreal value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    KisLiquifyProperties *props =
        config->liquifyProperties();

    props->setFlow(value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::liquifyBuildUpChanged(int value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    KisLiquifyProperties *props =
        config->liquifyProperties();

    props->setUseWashMode(!value); // 0 == build up mode / 1 == wash mode

    notifyConfigChanged();

    // we need to enable/disable flow slider
    updateLiquifyControls();
}

void KisToolTransformConfigWidget::liquifySpacingChanged(qreal value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    KisLiquifyProperties *props =
        config->liquifyProperties();

    props->setSpacing(value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::liquifySizePressureChanged(bool value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    KisLiquifyProperties *props =
        config->liquifyProperties();

    props->setSizeHasPressure(value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::liquifyAmountPressureChanged(bool value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    KisLiquifyProperties *props =
        config->liquifyProperties();

    props->setAmountHasPressure(value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::liquifyReverseDirectionChanged(bool value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    KisLiquifyProperties *props =
        config->liquifyProperties();

    props->setReverseDirection(value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::updateConfig(const ToolTransformArgs &config)
{
    blockUiSlots();

    if (config.mode() == ToolTransformArgs::FREE_TRANSFORM ||
        config.mode() == ToolTransformArgs::PERSPECTIVE_4POINT) {

        stackedWidget->setCurrentIndex(0);

        bool freeTransformIsActive = config.mode() == ToolTransformArgs::FREE_TRANSFORM;

        if (freeTransformIsActive) {
            freeTransformButton->setChecked(true);
        } else {
            perspectiveTransformButton->setChecked(true);
        }

        aXBox->setEnabled(freeTransformIsActive);
        aYBox->setEnabled(freeTransformIsActive);
        aZBox->setEnabled(freeTransformIsActive);
        foreach (QAbstractButton *button, m_rotationCenterButtons->buttons()) {
            button->setEnabled(freeTransformIsActive);
        }

        scaleXBox->setValue(config.scaleX() * 100.);
        scaleYBox->setValue(config.scaleY() * 100.);
        shearXBox->setValue(config.shearX());
        shearYBox->setValue(config.shearY());
        translateXBox->setValue(config.transformedCenter().x());
        translateYBox->setValue(config.transformedCenter().y());
        aXBox->setValue(radianToDegree(config.aX()));
        aYBox->setValue(radianToDegree(config.aY()));
        aZBox->setValue(radianToDegree(config.aZ()));
        aspectButton->setKeepAspectRatio(config.keepAspectRatio());
        cmbFilter->setCurrent(config.filterId());

        QPointF pt = m_transaction->currentConfig()->rotationCenterOffset();
        pt.rx() /= m_transaction->originalHalfWidth();
        pt.ry() /= m_transaction->originalHalfHeight();

        for (int i = 0; i < 9; i++) {
            if (qFuzzyCompare(m_handleDir[i].x(), pt.x()) &&
                qFuzzyCompare(m_handleDir[i].y(), pt.y())) {

                m_rotationCenterButtons->button(i)->setChecked(true);
                break;
            }
        }
    } else if (config.mode() == ToolTransformArgs::WARP) {

        stackedWidget->setCurrentIndex(1);
        warpButton->setChecked(true);

        if (config.defaultPoints()) {
            densityBox->setValue(config.numPoints());
        }

        cmbWarpType->setCurrentIndex((int)config.warpType());
        defaultRadioButton->setChecked(config.defaultPoints());
        customRadioButton->setChecked(!config.defaultPoints());
        densityBox->setEnabled(config.defaultPoints());
        customWarpWidget->setEnabled(!config.defaultPoints());

        updateLockPointsButtonCaption();

    } else if (config.mode() == ToolTransformArgs::CAGE) {

        // default UI options
        resetUIOptions();

        // we need at least 3 point before we can start actively deforming
        if (config.origPoints().size() >= 3)
        {
            cageTransformDirections->setText(i18n("Switch between editing and deforming cage"));
            cageAddEditRadio->setVisible(true);
            cageDeformRadio->setVisible(true);

            // update to correct radio button
            if (config.isEditingTransformPoints())
                cageAddEditRadio->setChecked(true);
            else
                 cageDeformRadio->setChecked(true);

        }

    } else if (config.mode() == ToolTransformArgs::LIQUIFY) {

        stackedWidget->setCurrentIndex(3);
        liquifyButton->setChecked(true);

        const KisLiquifyProperties *props =
            config.liquifyProperties();

        switch (props->mode()) {
        case KisLiquifyProperties::MOVE:
            liquifyMove->setChecked(true);
            break;
        case KisLiquifyProperties::SCALE:
            liquifyScale->setChecked(true);
            break;
        case KisLiquifyProperties::ROTATE:
            liquifyRotate->setChecked(true);
            break;
        case KisLiquifyProperties::OFFSET:
            liquifyOffset->setChecked(true);
            break;
        case KisLiquifyProperties::UNDO:
            liquifyUndo->setChecked(true);
            break;
        }

        updateLiquifyControls();
    }

    unblockUiSlots();
}

void KisToolTransformConfigWidget::updateLockPointsButtonCaption()
{
    ToolTransformArgs *config = m_transaction->currentConfig();

    if (config->isEditingTransformPoints()) {
        lockUnlockPointsButton->setText(i18n("Lock Points"));
    } else {
        lockUnlockPointsButton->setText(i18n("Unlock Points"));
    }
}

void KisToolTransformConfigWidget::setApplyResetDisabled(bool disabled)
{
    QAbstractButton *applyButton = buttonBox->button(QDialogButtonBox::Apply);
    QAbstractButton *resetButton = buttonBox->button(QDialogButtonBox::Reset);

    Q_ASSERT(applyButton);
    Q_ASSERT(resetButton);

    applyButton->setDisabled(disabled);
    resetButton->setDisabled(disabled);
}

void KisToolTransformConfigWidget::resetRotationCenterButtons()
{
    int checkedId = m_rotationCenterButtons->checkedId();

    if (checkedId >= 0 && checkedId <= 8) {
        // uncheck the current checked button
        m_rotationCenterButtons->button(9)->setChecked(true);
    }
}

bool KisToolTransformConfigWidget::workRecursively() const
{
    return chkWorkRecursively->isChecked();;
}

void KisToolTransformConfigWidget::setTooBigLabelVisible(bool value)
{
    tooBigLabelWidget->setVisible(value);
}

bool KisToolTransformConfigWidget::showDecorations() const
{
    return showDecorationsBox->isChecked();
}

void KisToolTransformConfigWidget::blockNotifications()
{
    m_notificationsBlocked++;
}

void KisToolTransformConfigWidget::unblockNotifications()
{
    m_notificationsBlocked--;
}

void KisToolTransformConfigWidget::notifyConfigChanged()
{
    if (!m_notificationsBlocked) {
        emit sigConfigChanged();
    }
    m_configChanged = true;
}

void KisToolTransformConfigWidget::blockUiSlots()
{
    m_uiSlotsBlocked++;
}

void KisToolTransformConfigWidget::unblockUiSlots()
{
    m_uiSlotsBlocked--;
}

void KisToolTransformConfigWidget::notifyEditingFinished()
{
    if (m_uiSlotsBlocked || m_notificationsBlocked || !m_configChanged) return;

    emit sigEditingFinished();
    m_configChanged = false;
}


void KisToolTransformConfigWidget::resetUIOptions()
{
    // reset tool states since we are done (probably can encapsulate this later)
    ToolTransformArgs *config = m_transaction->currentConfig();
    if (config->mode() == ToolTransformArgs::CAGE)
    {
        cageAddEditRadio->setVisible(false);
        cageAddEditRadio->setChecked(true);
        cageDeformRadio->setVisible(false);
        cageTransformDirections->setText(i18n("Create 3 points on the canvas to begin"));

        // ensure we are on the right options view
        stackedWidget->setCurrentIndex(2);
    }



}

void KisToolTransformConfigWidget::slotButtonBoxClicked(QAbstractButton *button)
{
    QAbstractButton *applyButton = buttonBox->button(QDialogButtonBox::Apply);
    QAbstractButton *resetButton = buttonBox->button(QDialogButtonBox::Reset);

    resetUIOptions();

    if (button == applyButton) {
        emit sigApplyTransform();
    }
    else if (button == resetButton) {
        emit sigResetTransform();
    }

}

void KisToolTransformConfigWidget::slotSetFreeTransformModeButtonClicked(bool value)
{
    if (!value) return;

    lblTransformType->setText(freeTransformButton->toolTip());

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setMode(ToolTransformArgs::FREE_TRANSFORM);
    emit sigResetTransform();
}

void KisToolTransformConfigWidget::slotSetWarpModeButtonClicked(bool value)
{
    if (!value) return;

    lblTransformType->setText(warpButton->toolTip());

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setMode(ToolTransformArgs::WARP);
    emit sigResetTransform();
}

void KisToolTransformConfigWidget::slotSetCageModeButtonClicked(bool value)
{
    if (!value) return;

    lblTransformType->setText(cageButton->toolTip());

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setMode(ToolTransformArgs::CAGE);
    emit sigResetTransform();
}

void KisToolTransformConfigWidget::slotSetLiquifyModeButtonClicked(bool value)
{
    if (!value) return;

    lblTransformType->setText(liquifyButton->toolTip());

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setMode(ToolTransformArgs::LIQUIFY);
    emit sigResetTransform();
}

void KisToolTransformConfigWidget::slotSetPerspectiveModeButtonClicked(bool value)
{
    if (!value) return;

    lblTransformType->setText(perspectiveTransformButton->toolTip());

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setMode(ToolTransformArgs::PERSPECTIVE_4POINT);
    emit sigResetTransform();
}

void KisToolTransformConfigWidget::slotFilterChanged(const KoID &filterId)
{
    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setFilterId(filterId.id());
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotRotationCenterChanged(int index)
{
    if (m_uiSlotsBlocked) return;

    if (index >= 0 && index <= 8) {
        ToolTransformArgs *config = m_transaction->currentConfig();

        double i = m_handleDir[index].x();
        double j = m_handleDir[index].y();

        config->setRotationCenterOffset(QPointF(i * m_transaction->originalHalfWidth(),
                                                j * m_transaction->originalHalfHeight()));

        notifyConfigChanged();
    }
}

void KisToolTransformConfigWidget::slotSetScaleX(double value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setScaleX(value / 100.);

    if (config->keepAspectRatio() &&
        !qFuzzyCompare(config->scaleX(), config->scaleY())) {

        blockNotifications();
        scaleYBox->setValue(sign(scaleYBox->value()) * value);
        unblockNotifications();
    }

    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetScaleY(double value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setScaleY(value / 100.);

    if (config->keepAspectRatio() &&
        !qFuzzyCompare(config->scaleX(), config->scaleY())) {

        blockNotifications();
        scaleXBox->setValue(sign(scaleXBox->value()) * value);
        unblockNotifications();
    }

    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetShearX(double value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setShearX(value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetShearY(double value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setShearY(value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetTranslateX(double value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setTransformedCenter(QPointF(value, config->transformedCenter().y()));
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetTranslateY(double value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setTransformedCenter(QPointF(config->transformedCenter().x(), value));
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetAX(double value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setAX(degreeToRadian(value));
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetAY(double value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setAY(degreeToRadian(value));
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetAZ(double value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setAZ(degreeToRadian(value));
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetKeepAspectRatio(bool value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setKeepAspectRatio(value);

    if (value) {
        if (qAbs(scaleXBox->value()) > qAbs(scaleYBox->value())) {
            blockNotifications();
            scaleYBox->setValue(sign(scaleYBox->value()) * scaleXBox->value());
            unblockNotifications();
        } else {
            blockNotifications();
            scaleXBox->setValue(sign(scaleXBox->value()) * scaleYBox->value());
            unblockNotifications();
        }
    }

    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetWarpAlpha(qreal value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();

    config->setAlpha((double)value);
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotSetWarpDensity(int value)
{
    if (m_uiSlotsBlocked) return;
    setDefaultWarpPoints(value);
}

void KisToolTransformConfigWidget::setDefaultWarpPoints(int pointsPerLine)
{
    if (pointsPerLine < 0) {
        pointsPerLine = DEFAULT_POINTS_PER_LINE;
    }

    int nbPoints = pointsPerLine * pointsPerLine;
    QVector<QPointF> origPoints(nbPoints);
    QVector<QPointF> transfPoints(nbPoints);
    qreal gridSpaceX, gridSpaceY;

    if (nbPoints == 1) {
        //there is actually no grid
        origPoints[0] = m_transaction->originalCenter();
        transfPoints[0] = m_transaction->originalCenter();
    }
    else if (nbPoints > 1) {
        gridSpaceX = m_transaction->originalRect().width() / (pointsPerLine - 1);
        gridSpaceY = m_transaction->originalRect().height() / (pointsPerLine - 1);
        double y = m_transaction->originalRect().top();
        for (int i = 0; i < pointsPerLine; ++i) {
            double x = m_transaction->originalRect().left();
            for (int j = 0 ; j < pointsPerLine; ++j) {
                origPoints[i * pointsPerLine + j] = QPointF(x, y);
                transfPoints[i * pointsPerLine + j] = QPointF(x, y);
                x += gridSpaceX;
            }
            y += gridSpaceY;
        }
    }

    ToolTransformArgs *config = m_transaction->currentConfig();

    config->setDefaultPoints(nbPoints > 0);
    config->setPoints(origPoints, transfPoints);

    notifyConfigChanged();
}

void KisToolTransformConfigWidget::activateCustomWarpPoints(bool enabled)
{
    ToolTransformArgs *config = m_transaction->currentConfig();

    densityBox->setEnabled(!enabled);
    customWarpWidget->setEnabled(enabled);

    if (!enabled) {
        config->setEditingTransformPoints(false);
        setDefaultWarpPoints(densityBox->value());
    } else {
        config->setEditingTransformPoints(true);
        setDefaultWarpPoints(0);
    }

    updateLockPointsButtonCaption();
}

void KisToolTransformConfigWidget::slotWarpDefaultPointsButtonClicked(bool value)
{
    if (m_uiSlotsBlocked) return;

    activateCustomWarpPoints(!value);
}

void KisToolTransformConfigWidget::slotWarpCustomPointsButtonClicked(bool value)
{
    if (m_uiSlotsBlocked) return;

    activateCustomWarpPoints(value);
}

void KisToolTransformConfigWidget::slotWarpResetPointsButtonClicked()
{
    if (m_uiSlotsBlocked) return;

    activateCustomWarpPoints(true);
}

void KisToolTransformConfigWidget::slotWarpLockPointsButtonClicked()
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->setEditingTransformPoints(!config->isEditingTransformPoints());

    if (config->isEditingTransformPoints()) {
        // reinit the transf points to their original value
        ToolTransformArgs *config = m_transaction->currentConfig();
        int nbPoints = config->origPoints().size();
        for (int i = 0; i < nbPoints; ++i) {
            config->transfPoint(i) = config->origPoint(i);
        }
    }

    updateLockPointsButtonCaption();
    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotWarpTypeChanged(int index)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();

    switch (index) {
    case KisWarpTransformWorker::AFFINE_TRANSFORM:
    case KisWarpTransformWorker::SIMILITUDE_TRANSFORM:
    case KisWarpTransformWorker::RIGID_TRANSFORM:
        config->setWarpType((KisWarpTransformWorker::WarpType)index);
        break;
    default:
        config->setWarpType(KisWarpTransformWorker::RIGID_TRANSFORM);
        break;
    }

    notifyConfigChanged();
}

void KisToolTransformConfigWidget::slotCageOptionsChanged(int value)
{
    if ( value == 0)
    {
        slotEditCagePoints(true);
    }
    else
    {
        slotEditCagePoints(false);
    }

    notifyEditingFinished();
}

void KisToolTransformConfigWidget::slotEditCagePoints(bool value)
{
    if (m_uiSlotsBlocked) return;

    ToolTransformArgs *config = m_transaction->currentConfig();
    config->refTransformedPoints() = config->origPoints();

    config->setEditingTransformPoints(value);
    notifyConfigChanged();
}
