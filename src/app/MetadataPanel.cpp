#include "app/MetadataPanel.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>

namespace {

QLabel *makeValueLabel(QWidget *parent)
{
    auto *label = new QLabel(QStringLiteral("–"), parent);
    label->setStyleSheet("color: #d3d1c7;");
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

} // namespace

namespace pictureviewer {

MetadataPanel::MetadataPanel(QWidget *parent)
    : QWidget(parent)
    , m_nameLabel(new QLabel(this))
    , m_dimensionsValue(makeValueLabel(this))
    , m_formatValue(makeValueLabel(this))
    , m_sizeValue(makeValueLabel(this))
    , m_modifiedValue(makeValueLabel(this))
    , m_createdValue(makeValueLabel(this))
{
    setStyleSheet("background-color: #242426; color: #b4b2a9;");
    setMinimumWidth(200);

    m_nameLabel->setStyleSheet("color: #e8e6df; font-weight: bold;");
    m_nameLabel->setWordWrap(true);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: #444441;");

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(6);
    form->addRow(tr("Rozměry"), m_dimensionsValue);
    form->addRow(tr("Formát"), m_formatValue);
    form->addRow(tr("Velikost"), m_sizeValue);
    form->addRow(tr("Změněno"), m_modifiedValue);
    form->addRow(tr("Vytvořeno"), m_createdValue);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(m_nameLabel);
    layout->addWidget(separator);
    layout->addLayout(form);
    layout->addStretch();
}

void MetadataPanel::setMetadata(const ImageInfo &info)
{
    const QFileInfo fileInfo(info.path);
    const QLocale locale;

    m_nameLabel->setText(fileInfo.fileName());
    m_dimensionsValue->setText(info.dimensionsString());
    m_formatValue->setText(info.format);

    if (info.fileSize > 1024 * 1024) {
        m_sizeValue->setText(QStringLiteral("%1 MB").arg(
            QString::number(info.fileSize / (1024.0 * 1024.0), 'f', 1)));
    } else {
        m_sizeValue->setText(QStringLiteral("%1 kB").arg(
            QString::number(info.fileSizeKb(), 'f', 1)));
    }

    m_modifiedValue->setText(locale.toString(fileInfo.lastModified(), QLocale::ShortFormat));
    m_createdValue->setText(locale.toString(fileInfo.birthTime(), QLocale::ShortFormat));
}

void MetadataPanel::clearMetadata()
{
    m_nameLabel->setText(QString());
    for (QLabel *label : {m_dimensionsValue, m_formatValue, m_sizeValue, m_modifiedValue, m_createdValue}) {
        label->setText(QStringLiteral("–"));
    }
}

} // namespace pictureviewer
