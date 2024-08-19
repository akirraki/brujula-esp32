import React from 'react';
import './Header.css';

function Header({ title, imageSrc }) {
    return (
        <header className="header">
            <img src={imageSrc} alt="Logo" className="header-image" />
            <h1 className="header-title">{title}</h1>
        </header>
    );
}

export default Header;