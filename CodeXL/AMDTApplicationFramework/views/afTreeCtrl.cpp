//==================================================================================
// Copyright (c) 2016 , Advanced Micro Devices, Inc.  All rights reserved.
//
/// \author AMD Developer Tools Team
/// \file afTreeCtrl.cpp
///
//==================================================================================

// Qt:
#include <QtWidgets>
#include <QWidget>

// Infra:
#include <AMDTOSWrappers/Include/osDebuggingFunctions.h>
#include <AMDTBaseTools/Include/gtAssert.h>
#include <AMDTApplicationComponents/Include/acIcons.h>
#include <AMDTApplicationComponents/Include/acFunctions.h>

// Local:
#include <AMDTApplicationFramework/Include/views/afTreeCtrl.h>


afTreeCtrl::afTreeCtrl(QWidget* pParent, int numberOfColumns, bool addPasteAction, bool addExpandCollapeAllActions) :
    acTreeCtrl(pParent, numberOfColumns, addPasteAction, addExpandCollapeAllActions),
    m_pHoveredItem(nullptr), m_isDragging(false), m_pDragTimer(nullptr)
{
    setDragDropMode(QAbstractItemView::InternalMove);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setDropIndicatorShown(true);
    m_pDragTimer = new QTimer();

    // Creating a local timer instead of using a global timer is safer and prevent collisions with the VS timers
    bool rc = connect(m_pDragTimer, SIGNAL(timeout()), this, SLOT(OnDragTimer()));
    GT_ASSERT(rc);
}

afTreeCtrl::~afTreeCtrl()
{
    if (m_pDragTimer != nullptr)
    {
        delete m_pDragTimer;
        m_pDragTimer = nullptr;
    }
}

/// event for potential drag start
void afTreeCtrl::mousePressEvent(QMouseEvent* pEvent)
{
    if (pEvent->button() == Qt::LeftButton)
    {
        m_startPos = pEvent->pos();
    }

    QTreeWidget::mousePressEvent(pEvent);
}

/// event for QDragObject creation
void afTreeCtrl::mouseMoveEvent(QMouseEvent* pEvent)
{
    if (pEvent->buttons() & Qt::LeftButton)
    {
        int distance = (pEvent->pos() - m_startPos).manhattanLength();

        if (distance >= QApplication::startDragDistance())
        {

            m_isDragging = false;
            emit DragAttempt(selectedItems(), m_isDragging);

            if (m_isDragging)
            {
                PerformDrag();
            }
        }
    }
}

void afTreeCtrl::PerformDrag()
{
    m_draggedItemsList.clear();

    if (selectedItems().size() > 0)
    {
        QTreeWidgetItem* pSelectedItem = selectedItems()[0];
        if (pSelectedItem != nullptr)
        {
            QMimeData* pMimeData = new QMimeData;

#pragma message ("TODO: do not submit before testing thie code:")
            QString plainText = pSelectedItem->text(0);
            pMimeData->setText(plainText);
            QByteArray array(plainText.toStdString().c_str());
            pMimeData->setData(QString("application/x-qabstractitemmodeldatalist"), array);
            QDrag* pDrag = new QDrag(this);
            pDrag->setMimeData(pMimeData);
            unsigned int iconPixelSize = acIconSizeToPixelSize(acGetRecommendedIconSize());
            QPixmap* pPixmap = new QPixmap(pSelectedItem->icon(0).pixmap(iconPixelSize, iconPixelSize));
            pDrag->setPixmap(*pPixmap);

            Qt::DropAction dropRes = pDrag->exec(Qt::MoveAction);
            if (dropRes & Qt::MoveAction)
            {
                foreach(QTreeWidgetItem* pItem, selectedItems())
                {
                    m_draggedItemsList << pItem;
                }
            }

            setAutoScroll(true);
        }
    }
}

void afTreeCtrl::AutoScroll()
{
    QModelIndex index = indexAt(mapFromGlobal(QCursor::pos()));
    QModelIndex indexB = indexBelow(index);
    QRect vRect = visualRect(indexB);

    if (height() <= vRect.y() + (vRect.height()))
    {
        scrollTo(indexB);
    }
    else if (y() >= visualRect(indexAbove(index)).y())
    {

        scrollTo(indexAbove(index));
    }
    else
    {
        GT_IF_WITH_ASSERT(m_pDragTimer != nullptr)
        {
            m_pDragTimer->stop();
        }
    }
}

void afTreeCtrl::dragEnterEvent(QDragEnterEvent* pEvent)
{
    QTreeWidgetItem* pItem = itemAt(pEvent->pos());

    if (pItem != nullptr)
    {
        pEvent->accept();
    }
    else
    {
        pEvent->ignore();
    }
}


void afTreeCtrl::OnDragTimer()
{
    AutoScroll();
}

void afTreeCtrl::dragMoveEvent(QDragMoveEvent* pEvent)
{
    QTreeWidgetItem* pItem = itemAt(pEvent->pos());

    if (pItem != nullptr)
    {
        // Handle the drag move event in UI elements that know the logic better
        emit TreeElementDragMoveEvent(pEvent);

        if ((m_pHoveredItem != nullptr) && (m_pHoveredItem != pItem))
        {
            m_pHoveredItem->setSelected(false);
        }

        m_pHoveredItem = pItem;

        GT_IF_WITH_ASSERT(m_pDragTimer != nullptr)
        {
            if (!m_pDragTimer->isActive() && hasAutoScroll())
            {
                m_pDragTimer->start(100);
            }
        }
    }
    else
    {
        pEvent->ignore();
    }
}

void afTreeCtrl::dropEvent(QDropEvent* pEvent)
{
    // Do not call the base class, we do not want the default tree drop action to be executed
    // QTreeWidget::dropEvent(pEvent);

    if (pEvent != nullptr)
    {
        const QMimeData* pMimeData = pEvent->mimeData();

        if (pMimeData != nullptr)
        {
            if (pMimeData->hasUrls())
            {
                emit TreeElementDropEvent(pEvent);
            }
            else
            {
                QByteArray encoded = pMimeData->data("application/x-qabstractitemmodeldatalist");
                QDataStream stream(&encoded, QIODevice::ReadOnly);

                while (!stream.atEnd())
                {
                    int row, col;
                    QMap<int, QVariant> roleDataMap;
                    stream >> row >> col >> roleDataMap;
                    QString dropped = roleDataMap[0].toString();

                    emit TreeElementDropEvent(pEvent);

                    m_pHoveredItem = nullptr;
                }
            }

        }
    }

    setAutoScroll(false);

    m_isDragging = false;
}

Qt::DropActions afTreeCtrl::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList afTreeCtrl::mimeTypes() const
{
    QStringList types;
    types << "application/x-qabstractitemmodeldatalist";
    return types;
}

QMimeData* afTreeCtrl::mimeData(const QModelIndexList& indexes) const
{
    QMimeData* mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (QModelIndex index, indexes)
    {
        if (index.isValid())
        {
            QString text = index.data(Qt::DisplayRole).toString();
            stream << text;
        }
    }

    mimeData->setData("application/x-qabstractitemmodeldatalist", encodedData);
    return mimeData;
}

