/* This file is part of the KDE project
 *
 * Copyright (C) 2017 Grigory Tantsevov <tantsevov@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "kis_watercolor_paintop_settings.h"

struct KisWatercolorPaintOpSettings::Private
{
    QList<KisUniformPaintOpPropertyWSP> uniformProperties;
};

KisWatercolorPaintOpSettings::KisWatercolorPaintOpSettings()
    : m_d(new Private)
{
}

KisWatercolorPaintOpSettings::~KisWatercolorPaintOpSettings()
{
}

bool KisWatercolorPaintOpSettings::paintIncremental()
{
    /**
     * The watercolor brush supports working in the
     * WASH mode only!
     */
    return false;
}

#include <brushengine/kis_slider_based_paintop_property.h>
#include "kis_paintop_preset.h"
#include "kis_paintop_settings_update_proxy.h"
#include "kis_watercolorop_option.h"
#include "kis_standard_uniform_properties_factory.h"

QList<KisUniformPaintOpPropertySP> KisWatercolorPaintOpSettings::uniformProperties(KisPaintOpSettingsSP settings)
{
    QList<KisUniformPaintOpPropertySP> props =
        listWeakToStrong(m_d->uniformProperties);

    if (props.isEmpty()) {
        {
            KisDoubleSliderBasedPaintOpPropertyCallback *prop =
                    new KisDoubleSliderBasedPaintOpPropertyCallback(
                        KisDoubleSliderBasedPaintOpPropertyCallback::Double,
                        "watercolor_gravityX",
                        i18n("GravityX"),
                        settings, 0);
            prop->setRange(-10.0, 10.0);
            prop->setSingleStep(1.0);
            prop->setReadCallback(
                        [](KisUniformPaintOpProperty *prop) {
                WatercolorOption option;
                option.readOptionSetting(prop->settings().data());

                prop->setValue(qreal(option.gravityX));
            });
            prop->setWriteCallback(
                        [](KisUniformPaintOpProperty *prop) {
                WatercolorOption option;
                option.readOptionSetting(prop->settings().data());
                option.gravityX = prop->value().toDouble();
                option.writeOptionSetting(prop->settings().data());
            });

            QObject::connect(preset()->updateProxy(), SIGNAL(sigSettingsChanged()), prop, SLOT(requestReadValue()));
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisDoubleSliderBasedPaintOpPropertyCallback *prop =
                    new KisDoubleSliderBasedPaintOpPropertyCallback(
                        KisDoubleSliderBasedPaintOpPropertyCallback::Double,
                        "watercolor_gravityY",
                        i18n("GravityY"),
                        settings, 0);
            prop->setRange(-10.0, 10.0);
            prop->setSingleStep(1.0);
            prop->setReadCallback(
                        [](KisUniformPaintOpProperty *prop) {
                WatercolorOption option;
                option.readOptionSetting(prop->settings().data());

                prop->setValue(qreal(option.gravityY));
            });
            prop->setWriteCallback(
                        [](KisUniformPaintOpProperty *prop) {
                WatercolorOption option;
                option.readOptionSetting(prop->settings().data());
                option.gravityY = prop->value().toDouble();
                option.writeOptionSetting(prop->settings().data());
            });

            QObject::connect(preset()->updateProxy(), SIGNAL(sigSettingsChanged()), prop, SLOT(requestReadValue()));
            prop->requestReadValue();
            props << toQShared(prop);
        }

        {
            KisUniformPaintOpPropertyCallback *prop =
                    new KisUniformPaintOpPropertyCallback(
                        KisUniformPaintOpPropertyCallback::Int,
                        "watercolor_brushType",
                        i18n("BrushType"),
                        settings, 0);
            prop->setReadCallback(
                        [](KisUniformPaintOpProperty *prop) {
                WatercolorOption option;
                option.readOptionSetting(prop->settings().data());

                prop->setValue(int(option.type));
            });
            prop->setWriteCallback(
                        [](KisUniformPaintOpProperty *prop) {
                WatercolorOption option;
                option.readOptionSetting(prop->settings().data());
                option.type = prop->value().toInt();
                option.writeOptionSetting(prop->settings().data());
            });

            QObject::connect(preset()->updateProxy(), SIGNAL(sigSettingsChanged()), prop, SLOT(requestReadValue()));
            prop->requestReadValue();
            props << toQShared(prop);
        }
    }

    {
        using namespace KisStandardUniformPropertiesFactory;

        Q_FOREACH (KisUniformPaintOpPropertySP prop, KisPaintOpSettings::uniformProperties(settings)) {
            if (prop->id() == opacity.id()) {
                props.prepend(prop);
            }
        }
    }

    return props;
}
