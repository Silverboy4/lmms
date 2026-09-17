/*
 * PluginBrowser.cpp - implementation of the plugin-browser
 *
 * Copyright (c) 2005-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include "PluginBrowser.h"

#include <algorithm>

#include <QColor>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "ConfigManager.h"
#include "embed.h"
#include "Engine.h"
#include "InstrumentTrack.h"
#include "Song.h"
#include "StringPairDrag.h"
#include "TrackContainerView.h"
#include "PluginFactory.h"

namespace lmms::gui
{


PluginBrowser::PluginBrowser( QWidget * _parent ) :
	SideBarWidget( tr( "Instrument Plugins" ),
				embed::getIconPixmap( "plugins" ).transformed( QTransform().rotate( 90 ) ), _parent )
{
	setWindowTitle( tr( "Instrument browser" ) );
	m_favorites = ConfigManager::inst()
		->value("pluginbrowser", "favorites")
		.split('\n', Qt::SkipEmptyParts);

	m_view = new QWidget( contentParent() );
	addContentWidget( m_view );

	auto view_layout = new QVBoxLayout(m_view);
	view_layout->setContentsMargins(5, 5, 5, 5);
	view_layout->setSpacing( 5 );

	auto hint = new QLabel( tr( "Drag an instrument "
					"into either the Song Editor, the "
					"Pattern Editor or an "
					"existing instrument track." ),
									m_view );
	hint->setWordWrap( true );

	auto searchBar = new QLineEdit(m_view);
	searchBar->setPlaceholderText(tr("Search name, description or author"));
	searchBar->setMaxLength(128);
	searchBar->setClearButtonEnabled(true);
	searchBar->addAction(embed::getIconPixmap("zoom"), QLineEdit::LeadingPosition);

	m_descTree = new QTreeWidget( m_view );
	m_descTree->setColumnCount( 1 );
	m_descTree->header()->setVisible( false );
	m_descTree->setIndentation( 10 );
	m_descTree->setSelectionMode( QAbstractItemView::NoSelection );

	connect( searchBar, &QLineEdit::textChanged,
			this, &PluginBrowser::onFilterChanged );

	view_layout->addWidget( hint );
	view_layout->addWidget( searchBar );
	view_layout->addWidget( m_descTree );

	addPlugins();

	m_descTree->header()->setSectionResizeMode( QHeaderView::ResizeToContents );
	updateRootVisibilities();
}


void PluginBrowser::updateRootVisibility( int rootIndex )
{
	QTreeWidgetItem * root = m_descTree->topLevelItem( rootIndex );
	bool hasVisibleChild = false;
	for (int itemIndex = 0; itemIndex < root->childCount(); ++itemIndex)
	{
		if (!root->child(itemIndex)->isHidden())
		{
			hasVisibleChild = true;
			break;
		}
	}
	root->setHidden( !hasVisibleChild );
}


void PluginBrowser::updateRootVisibilities()
{
	int rootCount = m_descTree->topLevelItemCount();
	for (int rootIndex = 0; rootIndex < rootCount; ++rootIndex)
	{
		updateRootVisibility( rootIndex );
	}
}


void PluginBrowser::onFilterChanged( const QString & filter )
{
	m_filter = filter.simplified();
	const auto terms = m_filter.split(' ', Qt::SkipEmptyParts);

	int rootCount = m_descTree->topLevelItemCount();
	for (int rootIndex = 0; rootIndex < rootCount; ++rootIndex)
	{
		QTreeWidgetItem * root = m_descTree->topLevelItem( rootIndex );

		int itemCount = root->childCount();
		for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
		{
			QTreeWidgetItem * item = root->child( itemIndex );
			auto descWidget = static_cast<PluginDescWidget*>(m_descTree->itemWidget(item, 0));
			const auto searchable = descWidget->searchText();

			bool matches = true;
			for (const auto& term : terms)
			{
				if (!searchable.contains(term, Qt::CaseInsensitive))
				{
					matches = false;
					break;
				}
			}
			item->setHidden( !matches );
		}
	}
	updateRootVisibilities();
}


void PluginBrowser::onFavoriteChanged( const QString & pluginId, bool favorite )
{
	if (favorite)
	{
		if (!m_favorites.contains(pluginId))
		{
			m_favorites.append(pluginId);
		}
	}
	else
	{
		m_favorites.removeAll(pluginId);
	}

	saveFavorites();
	addPlugins();
	onFilterChanged(m_filter);
}


void PluginBrowser::saveFavorites() const
{
	ConfigManager::inst()->setValue(
		"pluginbrowser", "favorites", m_favorites.join('\n'));
}


void PluginBrowser::addPlugins()
{
	const auto addRoot = [this](auto label)
	{
		const auto root = new QTreeWidgetItem();
		root->setText(0, label);
		m_descTree->addTopLevelItem(root);
		return root;
	};

	m_descTree->clear();

	const auto favoritesRoot = addRoot(tr("Favorites"));
	favoritesRoot->setExpanded(true);
	const auto lmmsRoot = addRoot("LMMS");
	lmmsRoot->setExpanded(true);

	const auto addWidget = [this](const auto& key, auto root, bool favorite)
	{
		const auto item = new QTreeWidgetItem();
		root->addChild(item);
		auto widget = new PluginDescWidget(key, favorite, m_descTree);
		connect(widget, &PluginDescWidget::favoriteChanged,
				this, &PluginBrowser::onFavoriteChanged,
				Qt::QueuedConnection);
		m_descTree->setItemWidget(item, 0, widget);
	};

	const auto addPlugin = [this, favoritesRoot, &addWidget](const auto& key, auto root)
	{
		const auto id = QString::fromUtf8(key.desc->name)
			+ QStringLiteral("::") + key.displayName();
		const bool favorite = m_favorites.contains(id);
		addWidget(key, root, favorite);
		if (favorite)
		{
			addWidget(key, favoritesRoot, true);
		}
	};

	auto descs = getPluginFactory()->descriptors(Plugin::Type::Instrument);
	std::sort(descs.begin(), descs.end(),
		[](auto d1, auto d2)
		{
			return qstricmp(d1->displayName, d2->displayName) < 0;
		}
	);

	for (const auto desc : descs)
	{
		if (desc->subPluginFeatures)
		{
			auto subPluginKeys = Plugin::Descriptor::SubPluginFeatures::KeyList{};
			desc->subPluginFeatures->listSubPluginKeys(desc, subPluginKeys);
			std::sort(subPluginKeys.begin(), subPluginKeys.end(),
				[](const auto& l, const auto& r)
				{
					return QString::compare(l.displayName(), r.displayName(), Qt::CaseInsensitive) < 0;
				}
			);

			const auto root = addRoot(desc->displayName);
			for (const auto& key : subPluginKeys) { addPlugin(key, root); }
		}
		else
		{
			addPlugin(Plugin::Descriptor::SubPluginFeatures::Key(desc, desc->name), lmmsRoot);
		}
	}

	updateRootVisibilities();
}




PluginDescWidget::PluginDescWidget(const PluginKey &_pk, bool favorite,
							QWidget * _parent ) :
	QWidget( _parent ),
	m_pluginKey( _pk ),
	m_logo( _pk.logo()->pixmap() ),
	m_mouseOver( false ),
	m_favorite( favorite )
{
	setFixedHeight( DEFAULT_HEIGHT );
	setMouseTracking( true );
	setCursor( Qt::PointingHandCursor );
	const auto description = _pk.desc->subPluginFeatures
		? _pk.description()
		: tr(_pk.desc->description);
	setToolTip(description + '\n'
		+ tr("Right-click to add or remove this plugin from favorites."));
}


QString PluginDescWidget::name() const
{
	return m_pluginKey.displayName();
}


QString PluginDescWidget::identifier() const
{
	return QString::fromUtf8(m_pluginKey.desc->name)
		+ QStringLiteral("::") + m_pluginKey.displayName();
}


QString PluginDescWidget::searchText() const
{
	QString text = m_pluginKey.displayName();
	text += QLatin1Char(' ');
	text += QString::fromUtf8(m_pluginKey.desc->name);
	text += QLatin1Char(' ');
	text += QString::fromUtf8(m_pluginKey.desc->author);
	text += QLatin1Char(' ');
	text += (m_pluginKey.desc->subPluginFeatures
		? m_pluginKey.description()
		: tr(m_pluginKey.desc->description));
	return text;
}


void PluginDescWidget::paintEvent( QPaintEvent * )
{
	QPainter p( this );

	QStyleOption o;
	o.initFrom( this );
	style()->drawPrimitive( QStyle::PE_Widget, &o, &p, this );

	const int s = 16 + ( 32 * ( qBound( 24, height(), 60 ) - 24 ) ) /
									( 60 - 24 );
	const QSize logo_size( s, s );
	QPixmap logo = m_logo.scaled( logo_size, Qt::KeepAspectRatio,
						Qt::SmoothTransformation );
	p.drawPixmap( 4, 4, logo );

	QFont f = p.font();
	if ( m_mouseOver )
	{
		f.setBold( true );
	}
	p.setFont( f );
	p.drawText( 10 + logo_size.width(), 15, m_pluginKey.displayName());

	if (m_favorite)
	{
		auto starFont = p.font();
		starFont.setBold(true);
		starFont.setPointSize(starFont.pointSize() + 2);
		p.setFont(starFont);
		p.setPen(QColor("#ff2da6"));
		p.drawText(QRect(width() - 26, 0, 22, height()),
				Qt::AlignCenter, QStringLiteral("★"));
	}
}


#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
void PluginDescWidget::enterEvent(QEnterEvent* event)
#else
void PluginDescWidget::enterEvent(QEvent* event)
#endif
{
	m_mouseOver = true;
	update();
	QWidget::enterEvent(event);
}


void PluginDescWidget::leaveEvent( QEvent * _e )
{
	m_mouseOver = false;
	update();
	QWidget::leaveEvent( _e );
}


void PluginDescWidget::mousePressEvent( QMouseEvent * _me )
{
	Engine::setDndPluginKey(&m_pluginKey);
	if ( _me->button() == Qt::LeftButton )
	{
		new StringPairDrag("instrument",
			QString::fromUtf8(m_pluginKey.desc->name), m_logo, this);
		leaveEvent( _me );
	}
}


void PluginDescWidget::contextMenuEvent(QContextMenuEvent* e)
{
	QMenu contextMenu(this);
	contextMenu.addAction(
		tr("Send to new instrument track"),
		[=, this]{ openInNewInstrumentTrack(m_pluginKey.desc->name); }
	);
	contextMenu.addSeparator();
	contextMenu.addAction(
		m_favorite ? tr("Remove from favorites") : tr("Add to favorites"),
		[this]
		{
			m_favorite = !m_favorite;
			update();
			emit favoriteChanged(identifier(), m_favorite);
		}
	);
	contextMenu.exec(e->globalPos());
}


void PluginDescWidget::openInNewInstrumentTrack(QString value)
{
	TrackContainer* tc = Engine::getSong();
	auto it = dynamic_cast<InstrumentTrack*>(Track::create(Track::Type::Instrument, tc));
	auto ilt = new InstrumentLoaderThread(this, it, value);
	ilt->start();
}


} // namespace lmms::gui
