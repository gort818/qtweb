#include "defaultmprisinterface.h"
#include "mainwindow.h"
#include "mprisinterface.h"
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWidget>

DefaultMprisInterface::DefaultMprisInterface(QWidget *parent)
    : MprisInterface(parent) {
  prevTitleId = "";
  prevArtUrl = "";
  titleInfoFetching = true;
}

void DefaultMprisInterface::setup(MainWindow *window) {

  MprisInterface::setup(window);

  workWithPlayer([this](MprisPlayer &p) {
    // Expose player capabilities.
    p.setCanQuit(true);
    p.setCanSetFullscreen(true);
    p.setCanPause(true);
    p.setCanPlay(true);
    p.setCanControl(true);
    p.setCanSeek(true);
    p.setMetadata(QVariantMap());

    connect(&p, SIGNAL(pauseRequested()), this, SLOT(pauseVideo()));
    connect(&p, SIGNAL(playRequested()), this, SLOT(playVideo()));
    connect(&p, SIGNAL(playPauseRequested()), this, SLOT(togglePlayPause()));
    connect(&p, SIGNAL(fullscreenRequested(bool)), this,
            SLOT(setFullScreen(bool)));
    connect(&p, SIGNAL(volumeRequested(double)), this,
            SLOT(setVideoVolume(double)));
    connect(&p, SIGNAL(setPositionRequested(QDBusObjectPath, qlonglong)), this,
            SLOT(setPosition(QDBusObjectPath, qlonglong)));
    connect(&p, SIGNAL(seekRequested(qlonglong)), this,
            SLOT(setSeek(qlonglong)));
  });

  // Connect slots and start timers.
  connect(&playerStateTimer, SIGNAL(timeout()), this,
          SLOT(playerStateTimerFired()));
  playerStateTimer.start(500);

  connect(&playerPositionTimer, SIGNAL(timeout()), this,
          SLOT(playerPositionTimerFired()));
  playerPositionTimer.start(170);

  connect(&metadataTimer, SIGNAL(timeout()), this, SLOT(metadataTimerFired()));
  metadataTimer.start(500);

  connect(&volumeTimer, SIGNAL(timeout()), this, SLOT(volumeTimerFired()));
  volumeTimer.start(220);


}

void DefaultMprisInterface::playVideo() {
  QString code = ("(function () {"
                  "var vid = document.querySelectorAll('video');"
                  "for (let i = 0, n = vid.length; i < n; ++i) { "
                  "if (vid[i].getAttribute('src')) vid[i].play();}"
                  "})();");
  qDebug() << "Player playing";
  webView()->page()->runJavaScript(code);
}

void DefaultMprisInterface::pauseVideo() {
  QString code = ("(function () {"
                  "var vid = document.querySelectorAll('video');"
                  "for (let i = 0, n = vid.length; i < n; ++i) { "
                  "if (vid[i].getAttribute('src')) vid[i].pause();}"
                  "})();");
  qDebug() << "Player paused";
  webView()->page()->runJavaScript(code);
}

void DefaultMprisInterface::togglePlayPause() {
  QString code = ("(function () {"
                  "var vid = document.querySelectorAll('video');"
                  "for (let i = 0, n = vid.length; i < n; ++i) { "
                  "if (vid[i].getAttribute('src')) { "
                  "if (vid[i].paused) vid[i].play();"
                  "else vid[i].pause();}}"
                  "})();");
  qDebug() << "Player toggled play/pause";
  webView()->page()->runJavaScript(code);
}

void DefaultMprisInterface::setVideoVolume(double volume) {
  QString code = ("(function () {"
                  "var vid = document.querySelectorAll('video');"
                  "for (let i = 0, n = vid.length; i < n; ++i) { "
                  "if (vid[i].getAttribute('src')) {var video = vid[i];} } "
                  "if (!video) return;"
                  "video.volume = " + 
                  QString::number(volume) + 
                  ";})();");
  qDebug() << "Player set volume to " << volume;
  webView()->page()->runJavaScript(code);
}

void DefaultMprisInterface::setFullScreen(bool fullscreen) {
  window()->setFullScreen(fullscreen);
}

void DefaultMprisInterface::getVolume(std::function<void(double)> callback) {
  QString code = ("(function () {"
                  "var vid = document.querySelectorAll('video');"
                  "for (let i = 0, n = vid.length; i < n; ++i) { "
                  "if (vid[i].getAttribute('src')) {var video = vid[i];} } "
                  "return (video) ? video.volume : -1;"
                  "})()");
  webView()->page()->runJavaScript(code, [callback](const QVariant &result) {
    callback(result.toDouble());
  });
}

void DefaultMprisInterface::getVideoPosition(
    std::function<void(qlonglong)> callback) {
  QString code = ("(function () {"
                  "var vid = document.querySelectorAll('video');"
                  "for (let i = 0, n = vid.length; i < n; ++i) { "
                  "if (vid[i].getAttribute('src')) {var video = vid[i];} } "
                  "return (video) ? video.currentTime : -1;"
                  "})()");
  webView()->page()->runJavaScript(code, [callback](const QVariant &result) {
    double seconds = result.toDouble();
    if (seconds < 0)
      callback(-1);
    else
      callback(seconds / 1e-6);
  });
}

void DefaultMprisInterface::setPosition(QDBusObjectPath trackId, qlonglong pos) {
  double seconds = static_cast<double>(pos);
  // double useconds= seconds/1e+6;
  double useconds = seconds / 1e+6;
  qDebug() << "set Position to " << useconds << " Seconds";
  QString code = ("(function () {"
                  "var vid = document.querySelectorAll('video');"
                  "for (let i = 0, n = vid.length; i < n; ++i) { "
                  "if (vid[i].getAttribute('src')) {"
                  "var video = vid[i]; break;} } "
                  "if (!video) return;"
                  "video.pause();"
                  "video.currentTime = " + 
                  QString::number(useconds) + 
                  ";"
                  "var sptimer = setInterval(function() {"
                  "if (video.paused && video.readyState == 4 || !video.paused) {"
                  "video.play();"
                  "clearInterval(sptimer);"
                  "}"
                  "}, 50);"
                  "})();");
  webView()->page()->runJavaScript(code);
}

void DefaultMprisInterface::setSeek(qlonglong seekPos) {
  double seconds = static_cast<double>(seekPos);
  // double useconds= seconds/1e+6;
  double useconds = seconds / 1e+6;
  qDebug() << "Seeking Position by " << useconds << " Seconds";
  QString code = ("(function () {"
                  "var vid = document.querySelectorAll('video');"
                  "for (let i = 0, n = vid.length; i < n; ++i) { "
                  "if (vid[i].getAttribute('src')) {"
                  "var video = vid[i]; break;} };"
                  "if (!video) return;"
                  "video.pause();"
                  "video.currentTime += " + 
                  QString::number(useconds) + 
                  ";"
                  "var sstimer = setInterval(function() {"
                  "if (video.paused && video.readyState == 4 || !video.paused) {"
                  "video.play();"
                  "clearInterval(sstimer);"
                  "}"
                  "}, 50);"
                  "})();");
  webView()->page()->runJavaScript(code);
}

void DefaultMprisInterface::getMetadata(
    std::function<void(qlonglong, const QString &, const QString &,
                       const QString &)>
        callback) {
  QString code =
      ("(function () {"
       "var vid = document.querySelectorAll('video');"
       "for (let i = 0, n = vid.length; i < n; ++i) {"
         "if (vid[i].getAttribute('src') && vid[i].duration) {"
           "var duration = vid[i].duration;"
        "}"
       "};"
       "try {"
         "var titleLabel = document.querySelector('title').innerText;"
       "} catch(err){"
         "var titleLabel = 'Playing Video'"
       "}"
       "var metadata = {};"
       "metadata.duration = (duration) ? duration : -1;"
       "metadata.nid = '';"
       "metadata.title = titleLabel;"
       "var art = '';"
       "metadata.arturl= art;"
       "return metadata;"
       "})()");
  webView()->page()->runJavaScript(code, [callback](const QVariant &result) {
    QVariantMap map = result.toMap();

    double seconds = map["duration"].toDouble();
    if (seconds < 0)
      seconds = -1;
    else
      seconds /= 1e-6;

    QString title = map["title"].toString();
    QString nid = map["nid"].toString();
    QString artUrl = map["arturl"].toString();

    callback(seconds, title, nid, artUrl);
  });
}

void DefaultMprisInterface::getVideoState(
    std::function<void(Mpris::PlaybackStatus)> callback) {
  QString code = ("(function () {"
                  "var vid = document.querySelectorAll('video');"
                  "for (let i = 0, n = vid.length; i < n; ++i) { "
                  "if (vid[i].getAttribute('src')) {"
                  "return (vid[i].paused) ? 'paused' : 'playing';"
                  "}} "
                  "return 'stopped';"
                  "})()");
  webView()->page()->runJavaScript(code, [callback](const QVariant &result) {
    QString resultString = result.toString();
    Mpris::PlaybackStatus status = Mpris::InvalidPlaybackStatus;
    if (resultString == "stopped")
      status = Mpris::Stopped;
    else if (resultString == "playing")
      status = Mpris::Playing;
    else if (resultString == "paused")
      status = Mpris::Paused;
    callback(status);
  });
}

void DefaultMprisInterface::playerStateTimerFired() {
  getVideoState([this](Mpris::PlaybackStatus state) {
    workWithPlayer([&](MprisPlayer &p) {
      p.setPlaybackStatus(state);
      p.setServiceName("QtWebFlix-Video");
    });
  });
}

void DefaultMprisInterface::playerPositionTimerFired() {
  getVideoPosition([this](qlonglong useconds) {
    workWithPlayer([&](MprisPlayer &p) { p.setPosition(useconds); });
  });
}

void DefaultMprisInterface::metadataTimerFired() {
  getMetadata([this](qlonglong lengthUseconds, const QString &title,
                     const QString &nid, const QString &artUrl) {
    workWithPlayer([&](MprisPlayer &p) {
      QVariantMap metadata;
      if (lengthUseconds >= 0) {
        metadata[Mpris::metadataToString(Mpris::Length)] =
            QVariant(lengthUseconds);
      }
      if (!title.isEmpty()) {
        metadata[Mpris::metadataToString(Mpris::Title)] = QVariant(title);
      }
      if (!nid.isEmpty()) {
        metadata[Mpris::metadataToString(Mpris::TrackId)] =
            QVariant("/com/video/title/" + nid);
        // QString artUrl = getArtUrl(nid);
        if (!artUrl.isEmpty()) {
          metadata[Mpris::metadataToString(Mpris::ArtUrl)] = QVariant(artUrl);
        }
      }
      p.setMetadata(metadata);
    });
  });
}

void DefaultMprisInterface::volumeTimerFired() {
  getVolume([this](double volume) {
    if (volume >= 0) {
      workWithPlayer([&](MprisPlayer &p) { p.setVolume(volume); });
    }
  });
}
