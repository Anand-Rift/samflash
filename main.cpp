// samflash - Odin-style flasher for Linux
// Odin mode  -> odin4 (BL/AP/CP/CSC)
// Recovery   -> heimdall (single recovery image)

#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QStackedWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <QFontDatabase>
#include <QProgressBar>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

struct Slot {
    QCheckBox *cb;
    QLineEdit *path;
    QPushButton *btn;
    QString flag;
};

static Slot makeSlot(QWidget *parent, QGridLayout *g, int row,
                     const QString &name, const QString &flag)
{
    Slot s;
    s.flag = flag;
    s.cb = new QCheckBox;
    s.btn = new QPushButton(name);
    s.btn->setMinimumWidth(80);
    s.path = new QLineEdit;
    g->addWidget(s.cb, row, 0);
    g->addWidget(s.btn, row, 1);
    g->addWidget(s.path, row, 2);

    QObject::connect(s.btn, &QPushButton::clicked, [=]() {
        QString f = QFileDialog::getOpenFileName(
            parent, "Select " + name, QString(),
            "Firmware (*.tar *.md5 *.tar.md5 *.img *.lz4 *.bin);;All files (*)");
        if (!f.isEmpty()) {
            s.path->setText(f);
            s.cb->setChecked(true);
        }
    });
    return s;
}

class SamFlash : public QWidget {
public:
    SamFlash() {
        setWindowTitle("SamFlash");
        resize(620, 520);

        auto *root = new QVBoxLayout(this);

        // mode switch
        auto *modeBox = new QHBoxLayout;
        rOdin = new QRadioButton("Odin mode (odin4)");
        rRec = new QRadioButton("Recovery mode (heimdall)");
        rOdin->setChecked(true);
        modeBox->addWidget(rOdin);
        modeBox->addWidget(rRec);
        modeBox->addStretch();
        root->addLayout(modeBox);

        // device status... IF THIS AINT HERE IT AINT ODIN... i mean look i want it to be as easy as odin and offring more features than odin too
        status = new QLabel("Device: not checked");
        root->addWidget(status);

        stack = new QStackedWidget;
        root->addWidget(stack);

        // --- page 0: odin ---
        auto *odinPage = new QWidget;
        auto *op = new QVBoxLayout(odinPage);
        auto *slotBox = new QGroupBox("Files");
        auto *g = new QGridLayout(slotBox);
        odinSlots << makeSlot(this, g, 0, "BL", "-b")
                  << makeSlot(this, g, 1, "AP", "-a")
                  << makeSlot(this, g, 2, "CP", "-c")
                  << makeSlot(this, g, 3, "CSC", "-s")
                  << makeSlot(this, g, 4, "UMS", "-u");   // yes bro....
        g->setColumnStretch(2, 1);
        op->addWidget(slotBox);

        auto *optBox = new QGroupBox("Options");
        auto *ol = new QHBoxLayout(optBox);
        cReboot = new QCheckBox("Auto reboot");
        cReboot->setChecked(true);
        cErase = new QCheckBox("Erase NAND");
        cRedl = new QCheckBox("Re-download");
        ol->addWidget(cReboot);
        ol->addWidget(cErase);
        ol->addWidget(cRedl);
        ol->addStretch();
        op->addWidget(optBox);
        stack->addWidget(odinPage);

        // ---recovery fuck--- heimdall will save the day lol
        auto *recPage = new QWidget;
        auto *rp = new QVBoxLayout(recPage);
        auto *rbox = new QGroupBox("Recovery image");
        auto *rg = new QGridLayout(rbox);
        recSlot = makeSlot(this, rg, 0, "Recovery", "--RECOVERY");
        vbmetaSlot = makeSlot(this, rg, 1, "VBMeta", "--VBMETA");
        rg->setColumnStretch(2, 1);
        rp->addWidget(rbox);
        cRecReboot = new QCheckBox("Reboot after flashing");
        cRecReboot->setChecked(true);
        rp->addWidget(cRecReboot);
        rp->addStretch();
        stack->addWidget(recPage);

        // log..*pls speed i need this*
        log = new QPlainTextEdit;
        log->setReadOnly(true);
        log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        root->addWidget(log, 1);

        // progress
        bar = new QProgressBar;
        bar->setRange(0, 100);
        bar->setValue(0);
        root->addWidget(bar);

        // buttons
        auto *bb = new QHBoxLayout;
        bStart = new QPushButton("Start");
        bStop = new QPushButton("Stop");
        bStop->setEnabled(false);
        bReset = new QPushButton("Reset");
        auto *bRefresh = new QPushButton("Refresh device");
        bb->addWidget(bRefresh);
        bb->addStretch();
        bb->addWidget(bStart);
        bb->addWidget(bStop);
        bb->addWidget(bReset);
        root->addLayout(bb);

        // wiring
        connect(rOdin, &QRadioButton::toggled, [=](bool on) {
            stack->setCurrentIndex(on ? 0 : 1);
            refreshDevice();
        });
        connect(bRefresh, &QPushButton::clicked, [=]() { refreshDevice(); });
        connect(bStart, &QPushButton::clicked, [=]() { startFlash(); });
        connect(bStop, &QPushButton::clicked, [=]() { proc.kill(); });
        connect(bReset, &QPushButton::clicked, [=]() { resetAll(); });

        proc.setProcessChannelMode(QProcess::MergedChannels);
        connect(&proc, &QProcess::readyRead, [=]() {
            QString txt = QString::fromLocal8Bit(proc.readAll());
            // progress lines often end with \r instead of \n, so split on both
            for (const QString &line : txt.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts)) {
                log->appendPlainText(line);
                QRegularExpressionMatch m = QRegularExpression("(\\d{1,3})\\s*%").match(line);
                if (m.hasMatch()) {
                    int v = qBound(0, m.captured(1).toInt(), 100);
                    if (bar->maximum() == 0) bar->setRange(0, 100);
                    bar->setValue(v);
                }
            }
        });
        connect(&proc, &QProcess::finished, [=](int code, QProcess::ExitStatus) {
            log->appendPlainText(code == 0 ? "\n== PASS ==" : QString("\n== FAIL (exit %1) ==").arg(code));
            setBusy(false);
            if (code == 0) bar->setValue(100);
            refreshDevice();
        });
        connect(&proc, &QProcess::errorOccurred, [=](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
                log->appendPlainText("Could not start the tool. Is it installed and in PATH? huh??");
                setBusy(false);
            }
        });

        auto *t = new QTimer(this);
        connect(t, &QTimer::timeout, [=]() { refreshDevice(); });
        t->start(5000);
        refreshDevice();
    }

private:
    QRadioButton *rOdin, *rRec;
    QLabel *status;
    QStackedWidget *stack;
    QList<Slot> odinSlots;
    Slot recSlot, vbmetaSlot;
    QCheckBox *cReboot, *cErase, *cRedl, *cRecReboot;
    QPlainTextEdit *log;
    QPushButton *bStart, *bStop, *bReset;
    QProcess proc;
    QString odinDev; // path from `odin4 -l` yeeee
    QProgressBar *bar;
    QTemporaryDir tmp; // holds .tar symlinks for .tar.md5 files

    void setBusy(bool b) {
        bStart->setEnabled(!b);
        bStop->setEnabled(b);
        bReset->setEnabled(!b);
        if (b) bar->setRange(0, 0);
        else { bar->setRange(0, 100); bar->setValue(0); }
    }

    void setStatus(const QString &text, bool ok) {
        status->setText(text);
        status->setStyleSheet(ok ? "color: #2e9e4f; font-weight: bold;"
                                 : "color: #c0392b; font-weight: bold;");
    }

    // .tar.md5 is just a .tar with an md5 line glued on the end..this is exactly what google ai search told me when i searched it..,
    // so a symlink without the .md5 is enough (no multi-GB copy)
    QString fixMd5(const QString &p) {
        if (!p.endsWith(".md5", Qt::CaseInsensitive)) return p;
        if (!tmp.isValid()) return p;
        QString name = QFileInfo(p).fileName();
        name.chop(4);
        if (!name.endsWith(".tar")) name += ".tar";
        QString link = tmp.path() + "/" + name;
        QFile::remove(link);
        if (QFile::link(p, link)) {
            log->appendPlainText("[md5] using " + link);
            return link;
        }
        log->appendPlainText("[md5] could not make symlink, passing original file");
        return p;
    }

    void refreshDevice() {
        if (proc.state() != QProcess::NotRunning) return;
        QProcess p;
        bool odin = rOdin->isChecked();
        if (odin) p.start("odin4", {"-l"});
        else p.start("heimdall", {"detect"});
        if (!p.waitForStarted(1000)) {
            setStatus(QString("%1 not found in PATH").arg(odin ? "odin4" : "heimdall"), false);
            return;
        }
        p.waitForFinished(2000);
        QString out = QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();

        if (odin) {
            odinDev = out.split('\n').value(0).trimmed();
            if (odinDev.isEmpty()) setStatus("No device (put phone in Download Mode)", false);
            else setStatus("Connected: " + odinDev, true);
        } else {
            if (p.exitCode() == 0) setStatus("Connected (heimdall)", true);
            else setStatus("No device (put phone in Download Mode)", false);
        }
    }

    void startFlash() {
        QString prog;
        QStringList args;

        if (rOdin->isChecked()) {
            prog = "odin4";
            for (const Slot &s : odinSlots) {
                if (s.cb->isChecked() && !s.path->text().isEmpty())
                    args << s.flag << fixMd5(s.path->text());
            }
            if (args.isEmpty()) {
                QMessageBox::warning(this, "SamFlash", "Pick at least one file.");
                return;
            }
            if (!odinDev.isEmpty()) args << "-d" << odinDev;
            if (cErase->isChecked()) args << "-e";
            if (cReboot->isChecked()) args << "--reboot";
            if (cRedl->isChecked()) args << "--redownload";
        } else {
            prog = "heimdall";
            args << "flash";
            for (const Slot &s : {recSlot, vbmetaSlot}) {
                if (s.cb->isChecked() && !s.path->text().isEmpty())
                    args << s.flag << s.path->text();
            }
            if (args.size() == 1) {
                QMessageBox::warning(this, "SamFlash", "Pick a recovery or vbmeta image.");
                return;
            }
            if (!cRecReboot->isChecked()) args << "--no-reboot";
        }

        log->appendPlainText("$ " + prog + " " + args.join(' ') + "\n");
        setBusy(true);
        proc.start(prog, args);
    }

    void resetAll() {
        for (const Slot &s : odinSlots) {
            s.path->clear();
            s.cb->setChecked(false);
        }
        for (const Slot &s : {recSlot, vbmetaSlot}) {
            s.path->clear();
            s.cb->setChecked(false);
        }
        log->clear();
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    SamFlash w;
    w.show();
    return app.exec();
}
