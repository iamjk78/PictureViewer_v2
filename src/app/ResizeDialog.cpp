#include "app/ResizeDialog.hpp"

#include "core/ImageResize.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace pictureviewer {

namespace {
constexpr int kMaxPercent = 200;
}

ResizeDialog::ResizeDialog(const QSize &originalSize, const QString &currentSuffix,
                           bool allowOverwrite, QWidget *parent)
    : QDialog(parent)
    , m_original(originalSize)
    , m_currentSuffix(canonicalSuffix(currentSuffix))
    , m_allowOverwrite(allowOverwrite && !currentSuffix.isEmpty())
{
    setWindowTitle(tr("Změnit velikost a formát"));
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(
        tr("Původní rozlišení: <b>%1 × %2 px</b>").arg(m_original.width()).arg(m_original.height()),
        this));

    // ── Velikost ────────────────────────────────────────────────────────────
    layout->addSpacing(6);
    layout->addWidget(new QLabel(tr("Velikost:"), this));

    auto *sliderRow = new QHBoxLayout;
    m_percentSlider = new QSlider(Qt::Horizontal, this);
    m_percentSlider->setRange(1, kMaxPercent);
    m_percentSlider->setValue(100);
    m_percentLabel = new QLabel(this);
    m_percentLabel->setMinimumWidth(48);
    m_percentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sliderRow->addWidget(m_percentSlider, 1);
    sliderRow->addWidget(m_percentLabel);
    layout->addLayout(sliderRow);

    auto *dimRow = new QHBoxLayout;
    m_widthSpin = new QSpinBox(this);
    m_heightSpin = new QSpinBox(this);
    for (QSpinBox *spin : {m_widthSpin, m_heightSpin}) {
        spin->setRange(1, kMaxResizeSide);
        spin->setSuffix(tr(" px"));
        spin->setKeyboardTracking(false);   // přepočet až po dopsání, ne po každé číslici
    }
    dimRow->addWidget(new QLabel(tr("Šířka:"), this));
    dimRow->addWidget(m_widthSpin);
    dimRow->addSpacing(12);
    dimRow->addWidget(new QLabel(tr("Výška:"), this));
    dimRow->addWidget(m_heightSpin);
    dimRow->addStretch();
    layout->addLayout(dimRow);

    m_keepRatio = new QCheckBox(tr("Zachovat poměr stran"), this);
    m_keepRatio->setChecked(true);
    layout->addWidget(m_keepRatio);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    // ── Formát ──────────────────────────────────────────────────────────────
    layout->addSpacing(6);
    layout->addWidget(new QLabel(tr("Formát:"), this));
    m_formatCombo = new QComboBox(this);
    if (!m_currentSuffix.isEmpty()) {
        m_formatCombo->addItem(tr("Beze změny (.%1)").arg(m_currentSuffix), QString());
    }
    for (const OutputFormat &format : writableOutputFormats()) {
        if (format.suffix != m_currentSuffix) {
            m_formatCombo->addItem(format.label, format.suffix);
        }
    }
    layout->addWidget(m_formatCombo);

    m_qualityRow = new QWidget(this);
    auto *qualityLayout = new QHBoxLayout(m_qualityRow);
    qualityLayout->setContentsMargins(0, 0, 0, 0);
    qualityLayout->addWidget(new QLabel(tr("Kvalita:"), m_qualityRow));
    m_qualitySlider = new QSlider(Qt::Horizontal, m_qualityRow);
    m_qualitySlider->setRange(10, 100);
    m_qualitySlider->setValue(92);
    m_qualityLabel = new QLabel(m_qualityRow);
    m_qualityLabel->setMinimumWidth(36);
    m_qualityLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    qualityLayout->addWidget(m_qualitySlider, 1);
    qualityLayout->addWidget(m_qualityLabel);
    layout->addWidget(m_qualityRow);

    // ── Tlačítka ────────────────────────────────────────────────────────────
    layout->addSpacing(8);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    m_overwriteButton = new QPushButton(tr("Přepsat originál"), this);
    m_saveNewButton = new QPushButton(tr("Uložit jako nový soubor…"), this);
    m_saveNewButton->setDefault(true);
    auto *cancel = new QPushButton(tr("Zrušit"), this);
    buttons->addWidget(m_overwriteButton);
    buttons->addWidget(m_saveNewButton);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);

    connect(m_percentSlider, &QSlider::valueChanged, this, &ResizeDialog::setSizeFromPercent);
    connect(m_widthSpin, &QSpinBox::valueChanged, this, &ResizeDialog::onWidthEdited);
    connect(m_heightSpin, &QSpinBox::valueChanged, this, &ResizeDialog::onHeightEdited);
    connect(m_qualitySlider, &QSlider::valueChanged, this, [this](int value) {
        m_qualityLabel->setText(QStringLiteral("%1").arg(value));
    });
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, [this] { refreshFormatState(); });
    connect(m_overwriteButton, &QPushButton::clicked, this, [this] {
        m_overwrite = true;
        accept();
    });
    connect(m_saveNewButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    m_qualityLabel->setText(QStringLiteral("%1").arg(m_qualitySlider->value()));
    setSizeFromPercent(100);
    refreshFormatState();
}

QSize ResizeDialog::targetSize() const
{
    return {m_widthSpin->value(), m_heightSpin->value()};
}

bool ResizeDialog::keepsFormat() const
{
    return m_formatCombo->currentData().toString().isEmpty();
}

QString ResizeDialog::outputSuffix() const
{
    return keepsFormat() ? m_currentSuffix : m_formatCombo->currentData().toString();
}

int ResizeDialog::quality() const
{
    return m_qualitySlider->value();
}

void ResizeDialog::setSizeFromPercent(int percent)
{
    if (m_updating) {
        return;
    }
    m_updating = true;
    const QSize size = sizeForPercent(m_original, percent);
    m_widthSpin->setValue(size.width());
    m_heightSpin->setValue(size.height());
    m_updating = false;
    refreshSizeInfo(false);
}

void ResizeDialog::onWidthEdited(int width)
{
    if (m_updating) {
        return;
    }
    m_updating = true;
    if (m_keepRatio->isChecked()) {
        m_heightSpin->setValue(proportionalSide(m_original.width(), m_original.height(), width));
    }
    m_updating = false;
    refreshSizeInfo(true);
}

void ResizeDialog::onHeightEdited(int height)
{
    if (m_updating) {
        return;
    }
    m_updating = true;
    if (m_keepRatio->isChecked()) {
        m_widthSpin->setValue(proportionalSide(m_original.height(), m_original.width(), height));
    }
    m_updating = false;
    refreshSizeInfo(true);
}

void ResizeDialog::refreshSizeInfo(bool syncSlider)
{
    const QSize size = targetSize();
    const int percent = static_cast<int>(std::lround(100.0 * size.width() / m_original.width()));

    // Posuvník sleduje šířku; nad jeho rozsahem zůstane na kraji, přesné číslo
    // ukazuje popisek. Při tažení posuvníkem se nepřenastavuje (zaokrouhlení
    // by u malých obrázků „škubalo" jezdcem).
    if (syncSlider) {
        m_updating = true;
        m_percentSlider->setValue(std::clamp(percent, 1, kMaxPercent));
        m_updating = false;
    }
    m_percentLabel->setText(QStringLiteral("%1 %").arg(percent));

    const bool allowed = isResizeTargetAllowed(size);
    const double megapixels = static_cast<double>(size.width()) * size.height() / 1e6;
    QString text = tr("Výsledek: <b>%1 × %2 px</b> (%3 Mpx)")
                       .arg(size.width()).arg(size.height()).arg(megapixels, 0, 'f', 1);
    if (!allowed) {
        text += tr("<br><span style=\"color:#c0392b\">Příliš velký obrázek (nejvýše %1 Mpx).</span>")
                    .arg(kMaxResizePixels / 1'000'000);
    } else if (size.width() > m_original.width() || size.height() > m_original.height()) {
        text += tr("<br>Zvětšení kvalitu nezlepší — obrázek bude jen rozmazanější.");
    }
    m_infoLabel->setText(text);
    m_overwriteButton->setEnabled(allowed);
    m_saveNewButton->setEnabled(allowed);
}

void ResizeDialog::refreshFormatState()
{
    // Přepsat originál jde jen se stejným formátem; jiný formát = vždy nový soubor.
    m_overwriteButton->setVisible(m_allowOverwrite && keepsFormat());

    const QString suffix = outputSuffix();
    bool lossy = false;
    for (const OutputFormat &format : writableOutputFormats()) {
        if (format.suffix == suffix) {
            lossy = format.hasQuality;
        }
    }
    m_qualityRow->setVisible(lossy);
}

} // namespace pictureviewer
