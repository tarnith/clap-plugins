#include <QCoreApplication>
#include <QSocketNotifier>
#include <QUrl>

#include "remote-gui-factory.hh"
#include "remote-gui-listener.hh"

namespace clap {

   RemoteGuiFactory::RemoteGuiFactory(int socket) {
      ////////////////////////
      // I/O initialization //
      ////////////////////////

#if defined(Q_OS_UNIX)
      _channel.reset(new clap::RemoteChannel(
         [this](const clap::RemoteChannel::Message &msg) { onMessage(msg); },
         false,
         *this,
         socket));

      _socketReadNotifier.reset(new QSocketNotifier(socket, QSocketNotifier::Read, this));
      connect(_socketReadNotifier.get(),
              &QSocketNotifier::activated,
              [this](QSocketDescriptor socket, QSocketNotifier::Type type) {
                 _channel->tryReceive();
                 if (!_channel->isOpen())
                    QCoreApplication::quit();
              });

      _socketWriteNotifier.reset(new QSocketNotifier(socket, QSocketNotifier::Write, this));
      connect(_socketWriteNotifier.get(),
              &QSocketNotifier::activated,
              [this](QSocketDescriptor socket, QSocketNotifier::Type type) {
                 _channel->trySend();
                 if (!_channel->isOpen()) {
                    QCoreApplication::quit();
                 }
              });

      _socketReadNotifier->setEnabled(true);
      _socketWriteNotifier->setEnabled(false);
#elif defined(Q_OS_WINDOWS)
      _remoteChannel.reset(new clap::RemoteChannel(
         [this](const clap::RemoteChannel::Message &msg) { onMessage(msg); },
         false,
         pipeInHandle,
         pipeOutHandle,
         [] { QCoreApplication::quit(); }));
#endif
   }

   void RemoteGuiFactory::modifyFd(int flags) {
      _socketReadNotifier->setEnabled(flags & CLAP_POSIX_FD_READ);
      _socketWriteNotifier->setEnabled(flags & CLAP_POSIX_FD_WRITE);
   }

   void RemoteGuiFactory::removeFd() {
      _socketReadNotifier.reset();
      _socketWriteNotifier.reset();
      QCoreApplication::quit();
   }

   uint32_t RemoteGuiFactory::createClient() {
      const uint32_t clientId = ++_nextClientId;
      auto context = std::make_unique<ClientContext>(clientId);
      context->listenner = std::make_unique<RemoteGuiListener>(*this, clientId);
      context->client = std::make_unique<Gui>(*context->listenner);
      _guiClients.emplace(clientId, std::move(context));
      return clientId;
   }

   void RemoteGuiFactory::destroyClient(uint32_t clientId) {
      _guiClients.erase(clientId);
   }

   Gui *RemoteGuiFactory::getClient(uint32_t clientId) const {
      auto it = _guiClients.find(clientId);
      if (it != _guiClients.end())
         return it->second->client.get();
      return nullptr;
   }

   void RemoteGuiFactory::onMessage(const RemoteChannel::Message &msg) {
      auto c = getClient(msg.header.clientId);

      switch (msg.header.type) {
      case messages::kCreateClientRequest: {
         assert(!c);
         messages::CreateClientRequest rq;
         messages::CreateClientResponse rp;
         msg.get(rq);

         auto clientId = createClient();
         rp.clientId = clientId;

         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kDestroyClientRequest: {
         if (!c)
            return;

         messages::DestroyClientRequest rq;
         messages::DestroyClientResponse rp;
         msg.get(rq);

         destroyClient(msg.header.clientId);

         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kDestroyRequest:
         assert(!c);
         messages::DestroyResponse rp;
         _channel->sendResponseAsync(msg, rp);
         qApp->quit();
         break;

      case messages::kUpdateTransportRequest: {
         if (!c)
            return;

         messages::UpdateTransportRequest rq;
         msg.get(rq);
         c->updateTransport(rq.transport);
         break;
      }

      case messages::kAddImportPathRequest: {
         if (!c)
            return;

         messages::AddImportPathRequest rq;
         msg.get(rq);
         c->addImportPath(rq.importPath);
         break;
      }

      case messages::kSetSkinRequest: {
         if (!c)
            return;

         messages::SetSkinRequest rq;
         msg.get(rq);
         c->setSkin(rq.skinUrl);
         break;
      }

      case messages::kDefineParameterRequest: {
         if (!c)
            return;

         messages::DefineParameterRequest rq;
         msg.get(rq);
         c->defineParameter(rq.info);
         break;
      }

      case messages::kSetParameterMappingIndicationRequest: {
         if (!c)
            return;

         messages::SetParameterMappingIndicationRequest rq;
         msg.get(rq);
         c->setParameterMappingIndication(rq.paramId, rq.hasIndication, rq.color, rq.label, rq.description);
         break;
      }

      case messages::kSetParameterAutomationIndicationRequest: {
         if (!c)
            return;

         messages::SetParameterAutomationIndicationRequest rq;
         msg.get(rq);
         c->setParameterAutomationIndication(rq.paramId, rq.automationState, rq.color);
         break;
      }

      case messages::kParameterValueRequest: {
         if (!c)
            return;

         messages::ParameterValueRequest rq;
         msg.get(rq);
         c->updateParameter(rq.paramId, rq.value, rq.modulation);
         break;
      }

      case messages::kCanResizeRequest: {
         messages::CanResizeRequest rq;
         messages::CanResizeResponse rp{false};
         msg.get(rq);
         if (c)
            rp.succeed = c->canResize();
         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kGetSizeRequest: {
         messages::GetSizeRequest rq;
         messages::GetSizeResponse rp{0, 0, false};
         msg.get(rq);
         if (c)
            rp.succeed = c->getSize(&rp.width, &rp.height);
         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kSetSizeRequest: {
         messages::SetSizeRequest rq;
         messages::SetSizeResponse rp{false};
         msg.get(rq);
         if (c)
            rp.succeed = c->setSize(rq.width, rq.height);
         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kRoundSizeRequest: {
         messages::RoundSizeRequest rq;
         messages::RoundSizeResponse rp{0, 0, false};

         if (c) {
            msg.get(rq);
            rp.width = rq.width;
            rp.height = rq.height;
            rp.succeed = c->roundSize(&rp.width, &rp.height);
         }

         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kSetScaleRequest: {
         messages::SetScaleRequest rq;
         messages::SetScaleResponse rp{false};
         msg.get(rq);
         if (c)
            rp.succeed = c->setScale(rq.scale);
         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kOpenWindowRequest: {
         messages::OpenWindowRequest rq;
         messages::AttachResponse rp{false};
         msg.get(rq);
         if (c)
            rp.succeed = c->openWindow();
         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kAttachX11Request: {
         messages::AttachX11Request rq;
         messages::AttachResponse rp{false};
         msg.get(rq);
         if (c)
            rp.succeed = c->attachX11(rq.window);
         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kAttachWin32Request: {
         messages::AttachWin32Request rq;
         messages::AttachResponse rp{false};
         msg.get(rq);
         if (c)
            rp.succeed = c->attachWin32(rq.hwnd);
         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kAttachCocoaRequest: {
         messages::AttachCocoaRequest rq;
         messages::AttachResponse rp{false};
         msg.get(rq);
         if (c)
            rp.succeed = c->attachCocoa(rq.nsView);
         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kShowRequest: {
         messages::ShowRequest rq;
         messages::ShowResponse rp;
         msg.get(rq);
         if (c)
            rp.succeed = c->show();
         _channel->sendResponseAsync(msg, rp);
         break;
      }

      case messages::kHideRequest: {
         messages::HideRequest rq;
         messages::HideResponse rp;
         msg.get(rq);
         if (c)
            rp.succeed = c->hide();
         _channel->sendResponseAsync(msg, rp);
         break;
      }
      }
   }

   RemoteGuiFactory::ClientContext::ClientContext(uint32_t cId) : clientId(cId) {}
   RemoteGuiFactory::ClientContext::~ClientContext() = default;
} // namespace clap