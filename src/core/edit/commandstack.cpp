#include "edit/commandstack.h"

void CommandStack::clear()
{
    m_steps.clear();
    m_index = 0;
    m_cleanIndex = 0;
}

void CommandStack::push(EditStep step)
{
    if (m_cleanIndex > m_index)
        m_cleanIndex = -1;  // the saved state was undone and is now replaced
    m_steps.resize(m_index);
    m_steps.append(std::move(step));
    ++m_index;
    if (m_steps.size() > m_limit) {
        m_steps.removeFirst();
        --m_index;
        // The saved state was the oldest one, which can no longer be reached.
        m_cleanIndex = m_cleanIndex > 0 ? m_cleanIndex - 1 : -1;
    }
}

const EditStep &CommandStack::undo()
{
    Q_ASSERT(canUndo());
    return m_steps.at(--m_index);
}

const EditStep &CommandStack::redo()
{
    Q_ASSERT(canRedo());
    return m_steps.at(m_index++);
}
