#include "macspoofinggroup.h"

#include <QPainter>
#include "graphicresources/fontmanager.h"
#include "graphicresources/imageresourcessvg.h"
#include "generalmessagecontroller.h"
#include "languagecontroller.h"
#include "utils/logger.h"
#include "dpiscalemanager.h"

namespace PreferencesWindow {

MacSpoofingGroup::MacSpoofingGroup(ScalableGraphicsObject *parent, const QString &desc, const QString &descUrl)
  : PreferenceGroup(parent, desc, descUrl)
{
    setFlags(flags() | QGraphicsItem::ItemClipsChildrenToShape);

    checkBoxEnable_ = new ToggleItem(this);
    checkBoxEnable_->setIcon(ImageResourcesSvg::instance().getIndependentPixmap("preferences/MAC_SPOOFING"));
    connect(checkBoxEnable_, &ToggleItem::stateChanged, this, &MacSpoofingGroup::onCheckBoxStateChanged);
    addItem(checkBoxEnable_);

    macAddressItem_ = new MacAddressItem(this);
    connect(macAddressItem_, &MacAddressItem::cycleMacAddressClick, this, &MacSpoofingGroup::onCycleMacAddressClick);
    addItem(macAddressItem_);

    comboBoxInterface_ = new ComboBoxItem(this);
    comboBoxInterface_ ->setCaptionFont(FontDescr(12, false));
    connect(comboBoxInterface_, &ComboBoxItem::currentItemChanged, this, &MacSpoofingGroup::onInterfaceItemChanged);
    addItem(comboBoxInterface_);

    autoRotateMacItem_ = new ToggleItem(this);
    connect(autoRotateMacItem_, &ToggleItem::stateChanged, this, &MacSpoofingGroup::onAutoRotateMacStateChanged);
    addItem(autoRotateMacItem_);

    hideItems(indexOf(macAddressItem_), indexOf(autoRotateMacItem_), DISPLAY_FLAGS::FLAG_NO_ANIMATION);

    connect(&LanguageController::instance(), &LanguageController::languageChanged, this, &MacSpoofingGroup::onLanguageChanged);
    onLanguageChanged();
}

void MacSpoofingGroup::setMacSpoofingSettings(const types::MacAddrSpoofing &macAddrSpoofing)
{
    if(macAddrSpoofing != settings_)
    {
        settings_ = macAddrSpoofing;

        checkBoxEnable_->setState(macAddrSpoofing.isEnabled);
        macAddressItem_->setMacAddress(macAddrSpoofing.macAddress);
        autoRotateMacItem_->setState(macAddrSpoofing.isAutoRotate);
        updateMode();
    }
}

void MacSpoofingGroup::setCurrentNetwork(const types::NetworkInterface &network)
{
    curNetwork_ = network;
}

void MacSpoofingGroup::onCheckBoxStateChanged(bool isChecked)
{
    qCDebug(LOG_USER) << "MAC Spoofing enabled: " << isChecked;
    settings_.isEnabled = isChecked;
    updateMode();
    emit macAddrSpoofingChanged(settings_);
}

void MacSpoofingGroup::onAutoRotateMacStateChanged(bool isChecked)
{
    qCDebug(LOG_USER) << "Auto-Rotate enabled: " << isChecked;
    settings_.isAutoRotate = isChecked;
    emit macAddrSpoofingChanged(settings_);
}

void MacSpoofingGroup::onInterfaceItemChanged(const QVariant &value)
{
    int interfaceIndex = value.toInt();

    for (int i = 0; i < settings_.networkInterfaces.size(); i++)
    {
        if (settings_.networkInterfaces[i].interfaceIndex == interfaceIndex)
        {
            settings_.selectedNetworkInterface = settings_.networkInterfaces[i];
            break;
        }
    }

    qCDebug(LOG_USER) << "Changed MAC Spoof Interface Selection: " << settings_.selectedNetworkInterface.interfaceName;
    emit macAddrSpoofingChanged(settings_);
}

void MacSpoofingGroup::onCycleMacAddressClick()
{
    if (curNetwork_.interfaceIndex == settings_.selectedNetworkInterface.interfaceIndex)
    {
        if (curNetwork_.interfaceIndex != -1)
        {
            emit cycleMacAddressClick();
        }
        else
        {
            qCDebug(LOG_BASIC) << "Cannot spoof on 'No Interface'";
            GeneralMessageController::instance().showMessage(
                "ERROR_ICON",
                tr("Cannot spoof on 'No Interface'"),
                tr("You can only spoof an existing adapter."),
                GeneralMessageController::tr(GeneralMessageController::kOk),
                "",
                "",
                std::function<void(bool)>(nullptr),
                std::function<void(bool)>(nullptr),
                std::function<void(bool)>(nullptr),
                GeneralMessage::kFromPreferences);
        }
    }
    else
    {
        qCDebug(LOG_BASIC) << "Cannot spoof the current interface -- must match selected interface";
        GeneralMessageController::instance().showMessage(
            "ERROR_ICON",
            tr("Cannot spoof the current interface"),
            tr("The current primary interface must match the selected interface to spoof."),
            GeneralMessageController::tr(GeneralMessageController::kOk),
            "",
            "",
            std::function<void(bool)>(nullptr),
            std::function<void(bool)>(nullptr),
            std::function<void(bool)>(nullptr),
            GeneralMessage::kFromPreferences);
    }
}

void MacSpoofingGroup::updateMode()
{
    if (settings_.isEnabled) {
        QString adapters = "";
        comboBoxInterface_->clear();
        for (int i = 0; i < settings_.networkInterfaces.size(); i++) {
            types::NetworkInterface interface = settings_.networkInterfaces[i];
            if (interface.interfaceName == "No Interface") {
                comboBoxInterface_->addItem(tr("No Interface"), interface.interfaceIndex);
            } else {
                comboBoxInterface_->addItem(interface.interfaceName, interface.interfaceIndex);
            }

            adapters += interface.interfaceName;

            if (i < settings_.networkInterfaces.size() - 1) {
                adapters += ", ";
            }
        }

        qCDebug(LOG_BASIC) << "Updating Spoofing adapter dropdown with: " << adapters;
        qCDebug(LOG_BASIC) << "Spoofing selection: " << settings_.selectedNetworkInterface.interfaceName;

        comboBoxInterface_->setCurrentItem(settings_.selectedNetworkInterface.interfaceIndex);
        showItems(indexOf(macAddressItem_), size() - 1);
    }
    else {
        hideItems(indexOf(macAddressItem_), size() - 1);
    }
}

void MacSpoofingGroup::onLanguageChanged()
{
    checkBoxEnable_->setCaption(tr("MAC Spoofing"));
    macAddressItem_->setCaption(tr("MAC Address"));
    comboBoxInterface_->setLabelCaption(tr("Interface"));
    autoRotateMacItem_->setCaption(tr("Auto-Rotate MAC"));
    updateMode();
}

} // namespace PreferencesWindow
