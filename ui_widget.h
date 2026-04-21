/********************************************************************************
** Form generated from reading UI file 'widget.ui'
**
** Created by: Qt User Interface Compiler version 5.12.10
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Widget
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *topButtonLayout;
    QPushButton *btnOpenXdma;
    QPushButton *btnStartReceive;
    QPushButton *btnStopReceive;
    QSpacerItem *topSpacer;
    QHBoxLayout *paramLayout;
    QLabel *labelWidth;
    QSpinBox *spinWidth;
    QLabel *labelHeight;
    QSpinBox *spinHeight;
    QLabel *labelThrottle;
    QSpinBox *spinThrottleMs;
    QLabel *labelChunk;
    QSpinBox *spinChunkKB;
    QSpacerItem *paramSpacer;
    QLabel *labelPreview;
    QPlainTextEdit *plainTextEdit;

    void setupUi(QWidget *Widget)
    {
        if (Widget->objectName().isEmpty())
            Widget->setObjectName(QString::fromUtf8("Widget"));
        Widget->resize(1100, 760);
        verticalLayout = new QVBoxLayout(Widget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        topButtonLayout = new QHBoxLayout();
        topButtonLayout->setObjectName(QString::fromUtf8("topButtonLayout"));
        btnOpenXdma = new QPushButton(Widget);
        btnOpenXdma->setObjectName(QString::fromUtf8("btnOpenXdma"));

        topButtonLayout->addWidget(btnOpenXdma);

        btnStartReceive = new QPushButton(Widget);
        btnStartReceive->setObjectName(QString::fromUtf8("btnStartReceive"));

        topButtonLayout->addWidget(btnStartReceive);

        btnStopReceive = new QPushButton(Widget);
        btnStopReceive->setObjectName(QString::fromUtf8("btnStopReceive"));

        topButtonLayout->addWidget(btnStopReceive);

        topSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        topButtonLayout->addItem(topSpacer);


        verticalLayout->addLayout(topButtonLayout);

        paramLayout = new QHBoxLayout();
        paramLayout->setObjectName(QString::fromUtf8("paramLayout"));
        labelWidth = new QLabel(Widget);
        labelWidth->setObjectName(QString::fromUtf8("labelWidth"));

        paramLayout->addWidget(labelWidth);

        spinWidth = new QSpinBox(Widget);
        spinWidth->setObjectName(QString::fromUtf8("spinWidth"));
        spinWidth->setMinimum(16);
        spinWidth->setMaximum(4096);
        spinWidth->setSingleStep(16);
        spinWidth->setValue(640);

        paramLayout->addWidget(spinWidth);

        labelHeight = new QLabel(Widget);
        labelHeight->setObjectName(QString::fromUtf8("labelHeight"));

        paramLayout->addWidget(labelHeight);

        spinHeight = new QSpinBox(Widget);
        spinHeight->setObjectName(QString::fromUtf8("spinHeight"));
        spinHeight->setMinimum(16);
        spinHeight->setMaximum(4096);
        spinHeight->setSingleStep(16);
        spinHeight->setValue(480);

        paramLayout->addWidget(spinHeight);

        labelThrottle = new QLabel(Widget);
        labelThrottle->setObjectName(QString::fromUtf8("labelThrottle"));

        paramLayout->addWidget(labelThrottle);

        spinThrottleMs = new QSpinBox(Widget);
        spinThrottleMs->setObjectName(QString::fromUtf8("spinThrottleMs"));
        spinThrottleMs->setMinimum(0);
        spinThrottleMs->setMaximum(1000);
        spinThrottleMs->setSingleStep(5);
        spinThrottleMs->setValue(40);

        paramLayout->addWidget(spinThrottleMs);

        labelChunk = new QLabel(Widget);
        labelChunk->setObjectName(QString::fromUtf8("labelChunk"));

        paramLayout->addWidget(labelChunk);

        spinChunkKB = new QSpinBox(Widget);
        spinChunkKB->setObjectName(QString::fromUtf8("spinChunkKB"));
        spinChunkKB->setMinimum(64);
        spinChunkKB->setMaximum(4096);
        spinChunkKB->setSingleStep(64);
        spinChunkKB->setValue(512);

        paramLayout->addWidget(spinChunkKB);

        paramSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        paramLayout->addItem(paramSpacer);


        verticalLayout->addLayout(paramLayout);

        labelPreview = new QLabel(Widget);
        labelPreview->setObjectName(QString::fromUtf8("labelPreview"));
        labelPreview->setMinimumSize(QSize(640, 360));
        labelPreview->setFrameShape(QFrame::StyledPanel);
        labelPreview->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(labelPreview);

        plainTextEdit = new QPlainTextEdit(Widget);
        plainTextEdit->setObjectName(QString::fromUtf8("plainTextEdit"));
        plainTextEdit->setReadOnly(true);

        verticalLayout->addWidget(plainTextEdit);


        retranslateUi(Widget);

        QMetaObject::connectSlotsByName(Widget);
    } // setupUi

    void retranslateUi(QWidget *Widget)
    {
        Widget->setWindowTitle(QApplication::translate("Widget", "XDMA C2H Video Viewer", nullptr));
        btnOpenXdma->setText(QApplication::translate("Widget", "\346\211\223\345\274\200 XDMA \345\271\266\350\207\252\346\243\200", nullptr));
        btnStartReceive->setText(QApplication::translate("Widget", "\345\274\200\345\247\213\346\216\245\346\224\266 C2H", nullptr));
        btnStopReceive->setText(QApplication::translate("Widget", "\345\201\234\346\255\242", nullptr));
        labelWidth->setText(QApplication::translate("Widget", "\345\256\275:", nullptr));
        labelHeight->setText(QApplication::translate("Widget", "\351\253\230", nullptr));
        labelThrottle->setText(QApplication::translate("Widget", "\350\212\202\346\265\201\346\227\266\351\227\264 (ms):", nullptr));
        labelChunk->setText(QApplication::translate("Widget", "Chunk (KB):", nullptr));
        labelPreview->setText(QApplication::translate("Widget", "Waiting for C2H video frames...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Widget: public Ui_Widget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WIDGET_H
