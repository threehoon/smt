#pragma once

#include "edit/programedits.h"
#include "model/program.h"

#include <QString>
#include <QVector>

// One undoable edit, stored as the whole program before and after. Program
// copies share every vector the edit did not touch (Qt implicit sharing), so
// a step costs roughly the size of what it changed.
struct EditStep
{
    QString text;  // for the Undo / Redo menu, e.g. "修改元件 RC-10K"
    Program before;
    Program after;
    QVector<IdRename> renames;  // before -> after
};

// Undo / redo history with a clean mark for the last save. QUndoStack is in
// QtWidgets, which this project does not use.
class CommandStack
{
public:
    explicit CommandStack(int limit = 200) : m_limit(limit) {}

    // Discards all steps; the new state is clean.
    void clear();
    // Adds a step that has just been applied. Discards the redo steps.
    void push(EditStep step);

    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index < m_steps.size(); }
    QString undoText() const { return canUndo() ? m_steps.at(m_index - 1).text : QString(); }
    QString redoText() const { return canRedo() ? m_steps.at(m_index).text : QString(); }

    // The step to revert / reapply; the caller applies it. Only call when
    // canUndo() / canRedo().
    const EditStep &undo();
    const EditStep &redo();

    // The current state is the one last marked clean (saved or opened).
    bool isClean() const { return m_index == m_cleanIndex; }
    void setClean() { m_cleanIndex = m_index; }

    int count() const { return m_steps.size(); }

private:
    QVector<EditStep> m_steps;
    int m_index = 0;       // steps [0, m_index) are applied
    int m_cleanIndex = 0;  // -1 when the clean state was discarded
    int m_limit;
};
