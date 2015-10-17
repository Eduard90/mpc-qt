#include <clocale>
#include <QApplication>
#include <QDir>
#include "main.h"
#include "storage.h"
#include "mainwindow.h"
#include "manager.h"

int main(int argc, char *argv[])
{
    // When (un)docking windows, some widgets may get transformed into native
    // widgets, causing painting glitches.  Tell Qt that we prefer non-native.
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QCoreApplication::setOrganizationDomain("cmdrkotori.mpc-qt");
    QApplication a(argc, argv);

    // Qt sets the locale in the QApplication constructor, but libmpv requires
    // the LC_NUMERIC category to be set to "C", so change it back.
    std::setlocale(LC_NUMERIC, "C");
    Flow f;
    if (!f.hasPrevious())
        return f.run();
    else
        return 0;
}

Flow::Flow(QObject *owner) :
    QObject(owner), process(NULL), mainWindow(NULL), playbackManager(NULL)
{
    process = new SingleProcess(this);
    if (process->hasPrevious()) {
        hasPrevious_ = true;
        return;
    }
    connect(process, SIGNAL(cmdlineReceived(QStringList)),
            this, SLOT(process_cmdlineRecieved(QStringList)));

    mainWindow = new MainWindow();
    playbackManager = new PlaybackManager(this);
    playbackManager->setMpvWidget(mainWindow->mpvWidget(), true);
    playbackManager->setPlaylistWindow(mainWindow->playlistWindow());

    // mainwindow -> manager
    connect(mainWindow, SIGNAL(severalFilesOpened(QList<QUrl>)),
            playbackManager, SLOT(openSeveralFiles(QList<QUrl>)));
    connect(mainWindow, SIGNAL(fileOpened(QUrl)),
            playbackManager, SLOT(openFile(QUrl)));
    connect(mainWindow, &MainWindow::dvdbdOpened,
            playbackManager, &PlaybackManager::playDiscFiles);
    connect(mainWindow, &MainWindow::streamOpened,
            playbackManager, &PlaybackManager::playStream);
    connect(mainWindow, SIGNAL(paused()),
            playbackManager, SLOT(pausePlayer()));
    connect(mainWindow, SIGNAL(unpaused()),
            playbackManager, SLOT(unpausePlayer()));
    connect(mainWindow, &MainWindow::stopped,
            playbackManager, &PlaybackManager::stopPlayer);
    connect(mainWindow, &MainWindow::stepBackward,
            playbackManager, &PlaybackManager::stepBackward);
    connect(mainWindow, &MainWindow::stepForward,
            playbackManager, &PlaybackManager::stepForward);
    connect(mainWindow, &MainWindow::speedDown,
            playbackManager, &PlaybackManager::speedDown);
    connect(mainWindow, &MainWindow::speedUp,
            playbackManager, &PlaybackManager::speedUp);
    connect(mainWindow, &MainWindow::speedReset,
            playbackManager, &PlaybackManager::speedReset);
    connect(mainWindow, &MainWindow::audioTrackSelected,
            playbackManager, &PlaybackManager::setAudioTrack);
    connect(mainWindow, &MainWindow::subtitleTrackSelected,
            playbackManager, &PlaybackManager::setSubtitleTrack);
    connect(mainWindow, &MainWindow::videoTrackSelected,
            playbackManager, &PlaybackManager::setVideoTrack);
    connect(mainWindow, &MainWindow::volumeChanged,
            playbackManager, &PlaybackManager::setVolume);
    connect(mainWindow, &MainWindow::volumeMuteChanged,
            playbackManager, &PlaybackManager::setMute);
    connect(mainWindow, &MainWindow::chapterPrevious,
            playbackManager, &PlaybackManager::navigateToPrevChapter);
    connect(mainWindow, &MainWindow::chapterNext,
            playbackManager, &PlaybackManager::navigateToNextChapter);
    connect(mainWindow, &MainWindow::chapterSelected,
            playbackManager, &PlaybackManager::navigateToChapter);
    connect(mainWindow, &MainWindow::timeSelected,
            playbackManager, &PlaybackManager::navigateToTime);

    // manager -> mainwindow
    connect(playbackManager, &PlaybackManager::timeChanged,
            mainWindow, &MainWindow::setTime);
    connect(playbackManager, &PlaybackManager::titleChanged,
            mainWindow, &MainWindow::setMediaTitle);
    connect(playbackManager, &PlaybackManager::chapterTitleChanged,
            mainWindow, &MainWindow::setChapterTitle);
    connect(playbackManager, &PlaybackManager::videoSizeChanged,
            mainWindow, &MainWindow::setVideoSize);
    connect(playbackManager, &PlaybackManager::stateChanged,
            mainWindow, &MainWindow::setPlaybackState);
    connect(playbackManager, &PlaybackManager::typeChanged,
            mainWindow, &MainWindow::setPlaybackType);
    connect(playbackManager, &PlaybackManager::chaptersAvailable,
            mainWindow, &MainWindow::setChapters);
    connect(playbackManager, &PlaybackManager::audioTracksAvailable,
            mainWindow, &MainWindow::setAudioTracks);
    connect(playbackManager, &PlaybackManager::videoTracksAvailable,
            mainWindow, &MainWindow::setVideoTracks);
    connect(playbackManager, &PlaybackManager::subtitleTracksAvailable,
            mainWindow, &MainWindow::setSubtitleTracks);
    connect(playbackManager, &PlaybackManager::fpsChanged,
            mainWindow, &MainWindow::setFps);
    connect(playbackManager, &PlaybackManager::avsyncChanged,
            mainWindow, &MainWindow::setAvsync);
    connect(playbackManager, &PlaybackManager::displayFramedropsChanged,
            mainWindow, &MainWindow::setDisplayFramedrops);
    connect(playbackManager, &PlaybackManager::decoderFramedropsChanged,
            mainWindow, &MainWindow::setDecoderFramedrops);

    // playlistwindow -> this.storage
    connect(mainWindow->playlistWindow(), &PlaylistWindow::importPlaylist,
            this, &Flow::importPlaylist);
    connect(mainWindow->playlistWindow(), &PlaylistWindow::exportPlaylist,
            this, &Flow::exportPlaylist);
}

Flow::~Flow()
{
    if (process) {
        delete process;
        process = NULL;
    }
    if (mainWindow) {
        storage.writeVList("playlists", mainWindow->playlistWindow()->tabsToVList());
        delete mainWindow;
        mainWindow = NULL;
    }
    if (playbackManager) {
        delete playbackManager;
        playbackManager = NULL;
    }
}

int Flow::run()
{
    mainWindow->playlistWindow()->tabsFromVList(storage.readVList("playlists"));
    mainWindow->show();
    if (!hasPrevious_)
        process_cmdlineRecieved(QCoreApplication::arguments());
    return qApp->exec();
}

bool Flow::hasPrevious()
{
    return hasPrevious_;
}

void Flow::process_cmdlineRecieved(const QStringList &args)
{
    QList<QUrl> files;
    QUrl url;
    foreach (QString s, args.mid(1)) {
        files << QUrl::fromUserInput(s, QDir::currentPath());
    }
    if (!files.empty()) {
        playbackManager->openSeveralFiles(files, true);
    }
}

void Flow::importPlaylist(QString fname)
{
    emit mainWindow->playlistWindow()->addSimplePlaylist(storage.readM3U(fname));
}

void Flow::exportPlaylist(QString fname, QStringList items)
{
    storage.writeM3U(fname, items);
}

