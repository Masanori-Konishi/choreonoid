#include "ManipulatorProgramViewBase.h"
#include "ManipulatorProgramItemBase.h"
#include "ManipulatorProgram.h"
#include "ManipulatorStatements.h"
#include <cnoid/ViewManager>
#include <cnoid/MenuManager>
#include <cnoid/TargetItemPicker>
#include <cnoid/TreeWidget>
#include <cnoid/Archive>
#include <cnoid/ConnectionSet>
#include <cnoid/BodyItem>
#include <cnoid/Buttons>
#include <QBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QProxyStyle>
#include <unordered_map>
#include "gettext.h"

using namespace std;
using namespace cnoid;

namespace {

class StatementItem : public QTreeWidgetItem
{
public:
    ManipulatorStatementPtr statement;
    ManipulatorProgramViewBase::Impl* viewImpl;

    StatementItem(ManipulatorStatement* statement, ManipulatorProgramViewBase::Impl* viewImpl);
    ~StatementItem();
    virtual QVariant data(int column, int role) const override;
};


class TreeWidgetStyle : public QProxyStyle
{
public:
    TreeWidgetStyle(QStyle* style) : QProxyStyle(style) { }
    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = 0) const;
};


class ScopedCounter
{
    int& counter;
public:
    ScopedCounter(int& counter) : counter(counter){
        ++counter;
    }
    ~ScopedCounter(){
        --counter;
    }
};

}

namespace cnoid {

class ManipulatorProgramViewBase::Impl : public TreeWidget
{
public:
    ManipulatorProgramViewBase* self;
    TargetItemPicker<ManipulatorProgramItemBase> targetItemPicker;
    ManipulatorProgramItemBasePtr programItem;
    unordered_map<ManipulatorStatementPtr, StatementItem*> statementItemMap;
    int statementItemOperationCallCounter;
    ScopedConnectionSet programItemConnections;
    ManipulatorStatementPtr currentStatement;
    Signal<void(ManipulatorStatement* statement)> sigCurrentStatementChanged;
    ManipulatorStatementPtr prevCurrentStatement;
    QLabel programNameLabel;
    ToolButton optionMenuButton;
    MenuManager optionMenuManager;
    MenuManager contextMenuManager;
    QHBoxLayout buttonBox[2];

    Impl(ManipulatorProgramViewBase* self);
    ~Impl();
    void setupWidgets();
    void onOptionMenuClicked();
    ScopedCounter scopedCounterOfStatementItemOperationCall();
    bool isDoingStatementItemOperation() const;
    void setProgramItem(ManipulatorProgramItemBase* item);
    void updateStatementTree();
    void onCurrentTreeWidgetItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void setCurrentStatement(ManipulatorStatement* statement);
    void onTreeWidgetItemClicked(QTreeWidgetItem* item, int /* column */);
    void onStatementAdded(ManipulatorProgram::iterator iter);
    void onStatementRemoved(ManipulatorStatement* statement);
    void forEachStatementInTreeEditEvent(
        const QModelIndex& parent, int start, int end,
        function<bool(ManipulatorProgram* program, int index, ManipulatorStatement* statement)> func);
    void onRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end);
    void onRowsInserted(const QModelIndex& parent, int start, int end);
    virtual void mousePressEvent(QMouseEvent* event) override;
    void showContextMenu(StatementItem* statementItem, QPoint globalPos);
    void cutStatement(ManipulatorStatement* statementItem);
    void copyStatement(ManipulatorStatement* statementItem);
    void pasteStatement(ManipulatorStatement* statementItem);
};

}


StatementItem::StatementItem(ManipulatorStatement* statement, ManipulatorProgramViewBase::Impl* viewImpl)
    : statement(statement),
      viewImpl(viewImpl)
{
    viewImpl->statementItemMap[statement] = this;

    setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
}


StatementItem::~StatementItem()
{
    viewImpl->statementItemMap.erase(statement);
}


QVariant StatementItem::data(int column, int role) const
{
    if(role == Qt::DisplayRole){
        return QString(statement->label(column).c_str());
    }
    return QTreeWidgetItem::data(column, role);
}


void TreeWidgetStyle::drawPrimitive
(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const
{
    if(element == QStyle::PE_IndicatorItemViewItemDrop && !option->rect.isNull()){
        QStyleOption opt(*option);
        opt.rect.setLeft(0);
        if(widget){
            opt.rect.setRight(widget->width());
        }
        QProxyStyle::drawPrimitive(element, &opt, painter, widget);
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}


ManipulatorProgramViewBase::ManipulatorProgramViewBase()
{
    impl = new Impl(this);
}


ManipulatorProgramViewBase::Impl::Impl(ManipulatorProgramViewBase* self)
    : self(self),
      targetItemPicker(self)
{
    setupWidgets();

    statementItemOperationCallCounter = 0;

    targetItemPicker.sigTargetItemChanged().connect(
        [&](ManipulatorProgramItemBase* item){ setProgramItem(item); });
}


ManipulatorProgramViewBase::~ManipulatorProgramViewBase()
{
    delete impl;
}


ManipulatorProgramViewBase::Impl::~Impl()
{

}


void ManipulatorProgramViewBase::Impl::setupWidgets()
{
    self->setDefaultLayoutArea(View::LEFT_TOP);
    self->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    auto vbox = new QVBoxLayout;
    vbox->setSpacing(0);

    auto hbox = new QHBoxLayout;
    hbox->addLayout(&buttonBox[0]);
    hbox->addStretch();
    optionMenuButton.setText(_("*"));
    optionMenuButton.sigClicked().connect([&](){ onOptionMenuClicked(); });
    hbox->addWidget(&optionMenuButton);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addLayout(&buttonBox[1]);
    hbox->addStretch();
    vbox->addLayout(hbox);

    programNameLabel.setFrameStyle(QFrame::Box | QFrame::Sunken);
    vbox->addWidget(&programNameLabel);

    // TreeWidget setup
    setColumnCount(3);
    setFrameShape(QFrame::NoFrame);
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDropIndicatorShown(true);
    setStyle(new TreeWidgetStyle(style()));
    
    auto& rheader = *header();
    rheader.setMinimumSectionSize(0);
    rheader.setStretchLastSection(false);
    rheader.setSectionResizeMode(0, QHeaderView::ResizeToContents);
    rheader.setSectionResizeMode(1, QHeaderView::ResizeToContents);
    rheader.setSectionResizeMode(2, QHeaderView::Stretch);
    
    sigCurrentItemChanged().connect(
        [&](QTreeWidgetItem* current, QTreeWidgetItem* previous){
            onCurrentTreeWidgetItemChanged(current, previous); });

    sigItemClicked().connect(
        [&](QTreeWidgetItem* item, int column){
            onTreeWidgetItemClicked(item, column); });

    sigRowsAboutToBeRemoved().connect(
        [&](const QModelIndex& parent, int start, int end){
            onRowsAboutToBeRemoved(parent, start, end); });
    
    sigRowsInserted().connect(
        [&](const QModelIndex& parent, int start, int end){
            onRowsInserted(parent, start, end);  });
    
    vbox->addWidget(this);
    
    self->setLayout(vbox);

    optionMenuManager.setNewPopupMenu(this);
    optionMenuManager.addItem(_("Refresh"))->sigTriggered().connect(
        [&](){ updateStatementTree(); });
}


void ManipulatorProgramViewBase::addStatementButton(QWidget* button, int row)
{
    impl->buttonBox[row].addWidget(button);
}


void ManipulatorProgramViewBase::onDeactivated()
{
    impl->currentStatement = nullptr;
    impl->prevCurrentStatement = nullptr;
}


void ManipulatorProgramViewBase::Impl::onOptionMenuClicked()
{
    optionMenuManager.popupMenu()->popup(optionMenuButton.mapToGlobal(QPoint(0,0)));
}


ScopedCounter ManipulatorProgramViewBase::Impl::scopedCounterOfStatementItemOperationCall()
{
    return ScopedCounter(statementItemOperationCallCounter);
}


bool ManipulatorProgramViewBase::Impl::isDoingStatementItemOperation() const
{
    return (statementItemOperationCallCounter > 0);
}


void ManipulatorProgramViewBase::Impl::setProgramItem(ManipulatorProgramItemBase* item)
{
    programItemConnections.disconnect();
    programItem = item;
    currentStatement = nullptr;

    bool accepted = self->onCurrentProgramItemChanged(item);
    if(!accepted){
        programItem = nullptr;
    }

    if(!programItem){
        programNameLabel.setStyleSheet("");
        programNameLabel.setText("---");

    } else {
        programItemConnections.add(
            programItem->sigNameChanged().connect(
                [&](const std::string&){ setProgramItem(programItem); }));

        programItemConnections.add(
            programItem->sigStatementAdded().connect(
                [&](ManipulatorProgram::iterator iter){ onStatementAdded(iter); }));

        programItemConnections.add(
            programItem->sigStatementRemoved().connect(
                [&](ManipulatorStatement* statement){ onStatementRemoved(statement); }));
        
        programNameLabel.setStyleSheet("font-weight: bold");
        programNameLabel.setText(programItem->name().c_str());

    }

    updateStatementTree();
}


void ManipulatorProgramViewBase::Impl::updateStatementTree()
{
    auto counter = scopedCounterOfStatementItemOperationCall();
    
    clear();
    statementItemMap.clear();
    
    if(programItem){
        auto program = programItem->program();
        for(auto& statement : *program){
            auto item = new StatementItem(statement, this);
            addTopLevelItem(item);
        }
    }
}


ManipulatorProgramItemBase* ManipulatorProgramViewBase::currentProgramItem()
{
    return impl->programItem;
}


ManipulatorStatement* ManipulatorProgramViewBase::currentStatement()
{
    return impl->currentStatement;
}


SignalProxy<void(ManipulatorStatement* statement)> ManipulatorProgramViewBase::sigCurrentStatementChanged()
{
    return impl->sigCurrentStatementChanged;
}


void ManipulatorProgramViewBase::Impl::onCurrentTreeWidgetItemChanged
(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    if(auto statementItem = dynamic_cast<StatementItem*>(previous)){
        prevCurrentStatement = statementItem->statement;
    }
    if(auto statementItem = dynamic_cast<StatementItem*>(current)){
        setCurrentStatement(statementItem->statement);
    }
}


void ManipulatorProgramViewBase::Impl::setCurrentStatement(ManipulatorStatement* statement)
{
    currentStatement = statement;
    self->onCurrentStatementChanged(statement);
    sigCurrentStatementChanged(statement);
    self->onCurrentStatementActivated(statement);
}


void ManipulatorProgramViewBase::onCurrentStatementChanged(ManipulatorStatement*)
{

}


void ManipulatorProgramViewBase::Impl::onTreeWidgetItemClicked(QTreeWidgetItem* item, int /* column */)
{
    if(auto statementItem = dynamic_cast<StatementItem*>(item)){
        auto statement = statementItem->statement;
        // If the clicked statement is different from the current one,
        // onCurrentTreeWidgetItemChanged is processed
        if(statement == prevCurrentStatement){
            self->onCurrentStatementActivated(statement);
        }
    }
}


void ManipulatorProgramViewBase::onCurrentStatementActivated(ManipulatorStatement*)
{

}


void ManipulatorProgramViewBase::Impl::onStatementAdded(ManipulatorProgram::iterator iter)
{
    auto counter = scopedCounterOfStatementItemOperationCall();
    auto item = new StatementItem(*iter, this);
    addTopLevelItem(item);
}


void ManipulatorProgramViewBase::Impl::onStatementRemoved(ManipulatorStatement* statement)
{
    auto counter = scopedCounterOfStatementItemOperationCall();
    auto iter = statementItemMap.find(statement);
    if(iter != statementItemMap.end()){
        auto& item = iter->second;
        takeTopLevelItem(indexOfTopLevelItem(item));
        delete item;
    }
}


void ManipulatorProgramViewBase::Impl::forEachStatementInTreeEditEvent
(const QModelIndex& parent, int start, int end,
 function<bool(ManipulatorProgram* program, int index, ManipulatorStatement* statement)> func)
{
    if(!programItem || isDoingStatementItemOperation()){
        return;
    }
    if(auto parentItem = itemFromIndex(parent)){
        return;
    }
    auto root = invisibleRootItem();
    auto program = programItem->program();
    bool updated = false;
    for(int i = start; i <= end; ++i){
        auto item = static_cast<StatementItem*>(root->child(i));
        if(func(program, i, item->statement)){
            updated = true;
        }
    }
    if(updated){
        programItem->suggestFileUpdate();
    }
}
    

void ManipulatorProgramViewBase::Impl::onRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    forEachStatementInTreeEditEvent(
        parent, start, end,
        [&](ManipulatorProgram* program, int, ManipulatorStatement* statement){
            return program->remove(statement);
        });
}


void ManipulatorProgramViewBase::Impl::onRowsInserted(const QModelIndex& parent, int start, int end)
{
    forEachStatementInTreeEditEvent(
        parent, start, end,
        [&](ManipulatorProgram* program, int index, ManipulatorStatement* statement){
            program->insert(program->begin() + index, statement);
            return true;
        });
}


void ManipulatorProgramViewBase::Impl::mousePressEvent(QMouseEvent* event)
{
    TreeWidget::mousePressEvent(event);

    if(event->button() == Qt::RightButton){
        auto item = dynamic_cast<StatementItem*>(itemAt(event->pos()));
        if(item){
            showContextMenu(item, event->globalPos());
        }
    }
}


void ManipulatorProgramViewBase::Impl::showContextMenu(StatementItem* statementItem, QPoint globalPos)
{
    contextMenuManager.setNewPopupMenu(this);

    auto statement = statementItem->statement;

    contextMenuManager.addItem(_("Cut"))
        ->sigTriggered().connect([=](){ cutStatement(statement); });

    contextMenuManager.addItem(_("Copy"))
        ->sigTriggered().connect([=](){ copyStatement(statement); });

    contextMenuManager.addItem(_("Paste"))
        ->sigTriggered().connect([=](){ pasteStatement(statement); });
    
    contextMenuManager.popupMenu()->popup(globalPos);
}


void ManipulatorProgramViewBase::Impl::cutStatement(ManipulatorStatement* statement)
{
    if(programItem){
        programItem->program()->remove(statement);
        programItem->sigStatementRemoved()(statement);
        programItem->suggestFileUpdate();
    }
}


void ManipulatorProgramViewBase::Impl::copyStatement(ManipulatorStatement* statementItem)
{

}


void ManipulatorProgramViewBase::Impl::pasteStatement(ManipulatorStatement* statementItem)
{

}


void ManipulatorProgramViewBase::insertIfStatement()
{
    auto& programItem = impl->programItem;
    if(programItem){
        auto iter = programItem->program()->append(new IfStatement);
        programItem->sigStatementAdded()(iter);
        programItem->suggestFileUpdate();
    }
}


void ManipulatorProgramViewBase::insertCallStatement()
{
    auto& programItem = impl->programItem;
    if(programItem){
        auto iter = programItem->program()->append(new CallStatement);
        programItem->sigStatementAdded()(iter);
        programItem->suggestFileUpdate();
    }
}


void ManipulatorProgramViewBase::insertSetSignalStatement()
{
    auto& programItem = impl->programItem;
    if(programItem){
        auto iter = programItem->program()->append(new SetSignalStatement);
        programItem->sigStatementAdded()(iter);
        programItem->suggestFileUpdate();
    }
}


void ManipulatorProgramViewBase::insertDelayStatement()
{
    auto& programItem = impl->programItem;
    if(programItem){
        auto iter = programItem->program()->append(new DelayStatement);
        programItem->sigStatementAdded()(iter);
        programItem->suggestFileUpdate();
    }
}


bool ManipulatorProgramViewBase::storeState(Archive& archive)
{
    impl->targetItemPicker.storeTargetItem(archive, "currentProgram");
    return true;
}


bool ManipulatorProgramViewBase::restoreState(const Archive& archive)
{
    impl->targetItemPicker.restoreTargetItemLater(archive, "currentProgram");
    return true;
}
