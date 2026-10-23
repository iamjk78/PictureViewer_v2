#pragma once

#include <QDialog>
#include <QSize>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QWidget;

namespace pictureviewer {

// Dialog "Změnit velikost a formát": cílové rozlišení (posuvník v % nebo
// zadáním šířky/výšky) a výstupní formát. Sám nic nezapisuje — jen vrací volbu.
class ResizeDialog : public QDialog
{
    Q_OBJECT

public:
    // currentSuffix: přípona zobrazeného souboru, pokud ji aplikace umí zapsat
    // (jinak prázdná — pak "stejný formát" není na výběr, a tedy ani přepsání
    // originálu). allowOverwrite: false u snímku, který originál nemá.
    ResizeDialog(const QSize &originalSize, const QString &currentSuffix,
                 bool allowOverwrite, QWidget *parent = nullptr);

    QSize targetSize() const;
    // Výstupní přípona (kanonická, např. "jpg"); při volbě "stejný formát"
    // je to currentSuffix.
    QString outputSuffix() const;
    bool keepsFormat() const;
    int quality() const;
    bool overwriteRequested() const { return m_overwrite; }

private:
    void setSizeFromPercent(int percent);
    void onWidthEdited(int width);
    void onHeightEdited(int height);
    void refreshSizeInfo(bool syncSlider);
    void refreshFormatState();

    QSize m_original;
    QString m_currentSuffix;
    bool m_allowOverwrite;
    bool m_overwrite = false;
    bool m_updating = false;   // brání zpětné vazbě mezi posuvníkem a poli

    QSlider *m_percentSlider = nullptr;
    QLabel *m_percentLabel = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QCheckBox *m_keepRatio = nullptr;
    QLabel *m_infoLabel = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QWidget *m_qualityRow = nullptr;
    QSlider *m_qualitySlider = nullptr;
    QLabel *m_qualityLabel = nullptr;
    QPushButton *m_overwriteButton = nullptr;
    QPushButton *m_saveNewButton = nullptr;
};

} // namespace pictureviewer
