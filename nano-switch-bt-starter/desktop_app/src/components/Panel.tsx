import type { ReactNode } from "react";

type PanelProps = {
  title: string;
  copy?: string;
  className?: string;
  actions?: ReactNode;
  children: ReactNode;
};

export function Panel({ title, copy, className, actions, children }: PanelProps) {
  return (
    <section className={className ? `panel ${className}` : "panel"}>
      <header className="panel-header">
        <div>
          <h2 className="panel-title">{title}</h2>
          {copy ? <p className="panel-copy">{copy}</p> : null}
        </div>
        {actions}
      </header>
      {children}
    </section>
  );
}
